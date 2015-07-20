#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libevaluate/rrvec_trec_eval.c,v 11.0 1992/07/21 18:20:34 chrisb Exp chrisb $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 
 * Expand a Bengali document vector by adding decompunds for every compound word found.
 * A decompound is a valid one only if it exists in the dictionary.
 * Also, applies sandhi rules whenever applicable.
 *
   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include <ctype.h>
#include <wchar.h>
#include <locale.h>
#include <langinfo.h>
#include "common.h"
#include "param.h"
#include "docdesc.h"
#include "dict.h"
#include "functions.h"
#include "smart_error.h"
#include "io.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "inst.h"
#include "dir_array.h"
#include "vector.h"
#include "inv.h"
#include "context.h"
#include "collstat.h"
#include "hn_unicode.h"
#include "bn_unicode.h"

#define DECOMPOUND_MIN_FREQ 20

static char *inv_file;           
static long inv_file_mode;
static int collstats_fd;
static char* collstat_file ;
static long collstat_mode ;
static long* freq;
static long collstats_num_freq;		// total number of unique terms in collection

static float alpha; // the weight of a decompound in the expanded vector
static float beta; // the weight of a decompound in the expanded vector
static char *dict_file;
static int dict_fd;
static long dict_mode;
static SM_INDEX_DOCDESC doc_desc;
static int contok_inst;
static char* locale;
static int wantVowelSandhi;
static long num_docs;
static char* lang;
static int langCode; // 0 <=> BN, 1 <=> HN

typedef struct {
	char* prefix;
	char* suffix;
	CON_WT prefixConwt;
	CON_WT suffixConwt;
	int pos;
	int len;
	int numMatches;		// whether its a prefix match, suffix match or both.
	float score;
}
SplitInfo;

typedef struct {
	SplitInfo array[MAX_TOKEN_LEN];
	int index;	// the array index
}SplitInfoArray;

typedef struct {
    int    valid_info;
	CON_WT *conwt_buff;
	int    conwt_buff_size;
	long   *ctype_len_buff;
	int    ctype_len_buff_size;
	long   num_words;
	SplitInfoArray splits;
}
STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;
static int inv_fd;
static float corr_thresh;
static int tokcon_inst;
static int both;
static char msg[8192];

static SPEC_PARAM spec_args[] = {
    {"vecexp_decomp.locale",	getspec_string, (char *) &locale},
    {"vecexp_decomp.alpha",	getspec_float, (char *) &alpha},
    {"vecexp_decomp.beta",	getspec_float, (char *) &beta},
    {"vecexp_decomp.corr_thresh",	getspec_float, (char *) &corr_thresh},
    {"vecexp_decomp.vowel_sandhi",	getspec_bool, (char *) &wantVowelSandhi},
    {"vecexp_decomp.dict_file",	getspec_dbfile, (char *) &dict_file},
    {"vecexp_decomp.dict_file.rmode", getspec_filemode,(char *) &dict_mode},
    {"vecexp_decomp.collstat_file",        getspec_dbfile,   (char *) &collstat_file},
    {"vecexp_decomp.collstat_file.rmode",  getspec_filemode, (char *) &collstat_mode},
    {"vecexp_decomp.ctype.0.inv_file", getspec_dbfile, (char *) &inv_file},
    {"vecexp_decomp.ctype.0.inv_file.rmode", getspec_filemode, (char *) &inv_file_mode},
    {"vecexp_decomp.lang", getspec_string, (char *)&lang},
    {"vecexp_decomp.both_compounds", getspec_int, (char *)&both},
    TRACE_PARAM ("vecexp_decomp.trace")
};

static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static long isInDict(wchar_t* word);
static int splitWord(STATIC_INFO* ip, wchar_t* word, int len, int pos, CON_WT* prefixConwt, CON_WT* suffixConwt);
static float insertDecompound(STATIC_INFO* ip, CON_WT* compound, CON_WT* decomp, float wt);
static int expandWordWithCompounds(STATIC_INFO* ip, CON_WT* conwtp);
static void vowelSandhi(wchar_t* prefix, int prefixLen, wchar_t* suffix, int suffixLen, long* suffixCon);
static int insertSplitInfo(SplitInfoArray* p, CON_WT* prefixConwt, CON_WT* suffixConwt, int pos, int len);

static int isConsonant(wchar_t p) {
	return langCode == 0? isBnConsonant(p) : isHnConsonant(p);	
}

static int isMatra(wchar_t p) {
	return langCode == 0? isBnMatra(p) : isHnMatra(p);	
}

static int isVowel(wchar_t p) {
	return langCode == 0? isBnVowel(p) : isHnVowel(p);	
}

// A dhatu is defined as a unit which has at least length 2+1/2... i.e.
// 2 consonants and one matra... 
static int isDhatu(wchar_t* word) {
	wchar_t* p;
	int c = 0;
	for (p = word; *p; p++) {
		if (isConsonant(*p))
			c += 2;
		else if (isMatra(*p))
			c += 1;
	}
	return c >= 5? 1 : 0;
}

static int insertSplitInfo(SplitInfoArray* p, CON_WT* prefixConwt, CON_WT* suffixConwt, int pos, int len) {
	SplitInfo* s;
	int numMatches = 0;
	float prefixFreq = 0, suffixFreq = 0;
   
	if (prefixConwt->con > 0) {
		numMatches++;
		prefixFreq = freq[prefixConwt->con];
	}

	if (suffixConwt->con > 0) {
		numMatches++;
		suffixFreq = freq[suffixConwt->con];
	}

	if (both & numMatches < 2)
		return 0;
	if (numMatches == 0)
		return 0;

	s = &(p->array[p->index]);
	s->pos = pos;
	s->len = len;
	s->numMatches = numMatches;
	s->prefixConwt.con = prefixConwt->con;
	s->prefixConwt.wt = prefixConwt->wt;
	s->suffixConwt.con = suffixConwt->con;
	s->suffixConwt.wt = suffixConwt->wt;
	s->score = (prefixFreq + suffixFreq);   ///(float)numMatches;

	p->index++;
	return numMatches;
}

int
init_vecexp_decomp (spec, unused)
SPEC *spec;
char *unused;
{
	DIR_ARRAY dir_array;
	int new_inst;
	STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering/leaving init_vecexp_decomp");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
	ip = &info[new_inst];
    ip->valid_info = 1;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

	// Set the locale to Bengali
    if (NULL == setlocale(LC_CTYPE, locale)) {
        print_error("init_vecexp_decomp", "Couldn't set locale...");
        return UNDEF;
    }
    else
        printf("Locale set to %s\n", nl_langinfo(CODESET));

	if (!strcmp(lang, "BN"))
		langCode = 0;
	else if (!strcmp(lang, "HN"))
		langCode = 1;
	else
		return UNDEF;

    if (UNDEF == lookup_spec_docdesc (spec, &doc_desc))
        return (UNDEF);

    /* Set param_prefix to be current parameter path for this ctype.
       This will then be used by the con_to_token routine to lookup
       parameters it needs. */
    if (UNDEF == (contok_inst =
                  doc_desc.ctypes[0].con_to_token->init_proc
                  (spec, "index.ctype.0.")))
        return (UNDEF);

	if (UNDEF == (dict_fd = open_dict (dict_file, dict_mode)))
		return UNDEF;

	if (! VALID_FILE (collstat_file)) {
		return UNDEF;
    }
    else {
		if (UNDEF == (collstats_fd = open_dir_array (collstat_file, collstat_mode)))
			return (UNDEF);

		// Read in collection frequencies
        ///dir_array.id_num = COLLSTAT_COLLFREQ; // Get the document frequency list from the file
        dir_array.id_num = COLLSTAT_TOTWT; // Get the collection frequency list from the file
        if (1 != seek_dir_array (collstats_fd, &dir_array) ||
            1 != read_dir_array (collstats_fd, &dir_array)) {
            collstats_num_freq = 0;
			freq = NULL;
			return UNDEF;
        }
        else {
            // Read from file successful. Allocate 'freq' array and dump the
            // contents of the file in this list
            collstats_num_freq = dir_array.num_list / sizeof (float);
		    if (NULL == (freq = (long *) malloc ((unsigned) dir_array.num_list)))
			        return (UNDEF);
	    	(void) bcopy (dir_array.list, (char *) freq, dir_array.num_list);
        }
		// Read in the total number of documents
        dir_array.id_num = COLLSTAT_NUMDOC;
        if (1 != seek_dir_array (collstats_fd, &dir_array) ||
            1 != read_dir_array (collstats_fd, &dir_array)) {
        }
        else {
            (void) bcopy (dir_array.list,
                          (char *)&num_docs,
                          sizeof(long));
        }
    }
    if (UNDEF == (inv_fd = open_inv(inv_file, inv_file_mode)))
		return (UNDEF);

	ip->ctype_len_buff_size = 16;
	ip->ctype_len_buff = Malloc(ip->ctype_len_buff_size, long);
	if (!ip->ctype_len_buff)
		return UNDEF;

	ip->conwt_buff_size = 8192;
	if (NULL == (ip->conwt_buff = Malloc(ip->conwt_buff_size, CON_WT)))
		return UNDEF;

	PRINT_TRACE (2, print_string, "Trace: leaving init_vecexp_decomp");
    return (new_inst);
}

static int wstrlen(wchar_t* word)
{
    wchar_t *p ;
    for (p = word; *p; p++) ;
	    return p - word;
}

// Apply a collection freq threshold filter to rule out the
// spelling mistake... (there are quite a few in FIRE corpus)
static int cfThresholdFilter(DICT_ENTRY *dict) {
	if (dict->con == 0)
		return 0;
	if (freq[dict->con] <= DECOMPOUND_MIN_FREQ)
		return 0;
	return 1;
}

static void prependChar(wchar_t* word, int len, wchar_t c) {
	wchar_t *p;
	for (p = &word[len+1]; p > word && p < &word[MAX_TOKEN_LEN]; p--) {
		*p = *(p-1);
	}
	*p = c;
}

// This function is called only if the prefix is a valid word
static void vowelSandhi(wchar_t* prefix, int prefixLen, wchar_t* suffix, int suffixLen, long* suffixCon) {
	static char suffixBuff[MAX_TOKEN_LEN * sizeof(wchar_t)];
	static char changedSuffixBuff[MAX_TOKEN_LEN * sizeof(wchar_t)];
	wchar_t firstSuffixChar, lastPrefixChar;
	long con;
	int  status;
	wchar_t AA = langCode == 0? bn_AA: hn_AA;
	wchar_t aa = langCode == 0? bn_aa: hn_aa;
	wchar_t E = langCode == 0? bn_E: hn_E;
	wchar_t O = langCode == 0? bn_O: hn_O;
	wchar_t i = langCode == 0? bn_i: hn_i;
	wchar_t u = langCode == 0? bn_u: hn_u;

	if (!wantVowelSandhi)
		return;

	status = wcstombs(suffixBuff, suffix, MAX_TOKEN_LEN * sizeof(wchar_t));
	if (status == UNDEF)
		return;

	lastPrefixChar = prefix[prefixLen-1];
	firstSuffixChar = *suffix;

	if (isConsonant(lastPrefixChar) && isConsonant(firstSuffixChar))
		return;
	else if (lastPrefixChar == AA && isConsonant(firstSuffixChar))
		// Insert the vowel AA
		prependChar(suffix, suffixLen, aa);
	else if (isConsonant(lastPrefixChar) && firstSuffixChar == AA)
		*suffix = aa;
	else if (firstSuffixChar == E)
		*suffix = i;
	else if (firstSuffixChar == O)
		*suffix = u;

	status = wcstombs(changedSuffixBuff, suffix, MAX_TOKEN_LEN * sizeof(wchar_t));
	if (status == UNDEF)
		return;

	// Check if the new suffix is a valid word in the 
	// dictionary i.e. it's got a valid concept no.
	if ((con = isInDict(suffix)) > 0) {
		snprintf(msg, sizeof(msg), "DC S :Changed suffix %s into %s", suffixBuff, changedSuffixBuff);
    	PRINT_TRACE (4, print_string, msg);
	}
	*suffixCon = con;
}

/* Returns the concept number of word, if the word is in dictionary.
 * Returns 0 otherwise
 *         UNDEF on error
 */
static long isInDict(wchar_t* word) {
	static char buff[MAX_TOKEN_LEN * sizeof(wchar_t)];
	DICT_ENTRY dict;
    char *stemmedWord;
	int  status, i;

	status = wcstombs(buff, word, MAX_TOKEN_LEN * sizeof(wchar_t));
	if (status == UNDEF)
		return UNDEF;

	// check if the word is in the vocabulary, if not discard it
    dict.token = buff;
    dict.con = UNDEF;
    if (UNDEF == (status = (seek_dict (dict_fd, &dict))))
		return (UNDEF);
	if (status > 0) {
		// the word is dictionary
        if (1 != read_dict (dict_fd, &dict))
	        return (UNDEF);
	}
	else
		return 0;

	if (cfThresholdFilter(&dict) == 0)
		return 0;
	if (isDhatu(word) == 0)
		return 0;
	return dict.con;
}

/* Splits up a word into prefix and suffix
 * Modifies the end of the prefix and 
 * the start of the suffix, by sandhi rules (if applicable).
 * Returns 1 if the modified prefix or the suffix exists in the dictionary.
 *    ''   UNDEF on error
 * */
static int splitWord(STATIC_INFO* ip, wchar_t* word, int len, int pos, CON_WT* prefixConwt, CON_WT* suffixConwt) {
	static wchar_t prefixBuff[MAX_TOKEN_LEN];
	static wchar_t suffixBuff[MAX_TOKEN_LEN];
	long con;

	// split up into [0...pos-1] [pos...len-1]
	memcpy(prefixBuff, word, sizeof(wchar_t)*pos);
	prefixBuff[pos] = 0;
	memcpy(suffixBuff, word+pos, sizeof(wchar_t)*(len-pos));
	suffixBuff[len-pos] = 0;

	if ((con = isInDict(prefixBuff)) == UNDEF)
		return UNDEF;
	else
		prefixConwt->con = con;

	if ((con = isInDict(suffixBuff)) == UNDEF) {
		return UNDEF;
	}
	else {
		suffixConwt->con = con;
	}

	// if the suffix is not a valid word, but the prefix is, 
	// it might be the case of a sandhi...
	// e.g. surya + o-kar doy
	// in this case, it is a vowel guna sandhi where the o-kar needs
	// to be changed to the vowel 'u' and then the word be
	// searched again in the dictionary
	if (prefixConwt->con > 0 && isDhatu(prefixBuff))
		vowelSandhi(prefixBuff, pos, suffixBuff, len-pos, &(suffixConwt->con));	

	// store the split results in the array
	return insertSplitInfo(&(ip->splits), prefixConwt, suffixConwt, pos, len);
}

/* Expands a Bengali word with compounds.
 */
static int expandWordWithCompounds(STATIC_INFO* ip, CON_WT* conwtp) {
    char     *compoundWord;
	wchar_t  compoundWordWC[MAX_TOKEN_LEN];
	int      pos, len, median;
	int      status;
	CON_WT   prefixConwt, suffixConwt;
	SplitInfo* s, *max;
	char*    decompound;
	float    corr;

	// reset the split infos
	memset(&(ip->splits), 0, sizeof(ip->splits));

	prefixConwt.con = 0; suffixConwt.con = 0;
	prefixConwt.wt = conwtp->wt; suffixConwt.wt = conwtp->wt;

	if (UNDEF == doc_desc.ctypes[0].con_to_token->proc (&(conwtp->con), &compoundWord, contok_inst)) {
		return UNDEF;
	}
	
    // Convert the multibyte sequence into wide character string
	mbstowcs(compoundWordWC, compoundWord, MAX_TOKEN_LEN);

	// Try splitting up the compound word in various places
	len = wstrlen(compoundWordWC);

	// Median length splitting...
	median = len/2;
	for (pos = 0; median-pos > 1 && median+pos < len; pos++) {
		splitWord(ip, compoundWordWC, len, median-pos, &prefixConwt, &suffixConwt);
		if (pos == 0)
			continue;
		splitWord(ip, compoundWordWC, len, median+pos, &prefixConwt, &suffixConwt); 
	}

	// Now choose the most effective decompound.
	// For now, we maximize the numMatches... (REVISIT)
	// For a tie in the numMatches, choose the
	// split which results in a larger prefix
	// (or a shorter suffix). The condition is that pos
	// should be as large as possible
	max = &ip->splits.array[0];
	for (s = &(ip->splits.array[1]); s < &(ip->splits.array[ip->splits.index]); s++) {
		if (s->score > max->score) {
			max = s;
		}
		else if (s->score == max->score) {
			if (s->pos > max->pos) {
				max = s;
			}
		}
	}

	if (max->numMatches == 0 || max->score < freq[conwtp->con]) {
		if (sm_trace >= 4) {
			snprintf(msg, sizeof(msg), "DC NO: Couldn't decompound word %s", compoundWord);
    		PRINT_TRACE (4, print_string, msg);
		}
		return 0;
	}

	if (sm_trace >= 4) {
		if (max->prefixConwt.con > 0 && max->suffixConwt.con > 0) {
			if (UNDEF == doc_desc.ctypes[0].con_to_token->proc (&(max->prefixConwt.con), &max->prefix, contok_inst))
				return UNDEF;
			if (UNDEF == doc_desc.ctypes[0].con_to_token->proc (&(max->suffixConwt.con), &max->suffix, contok_inst))
				return UNDEF;
			snprintf(msg, sizeof(msg), "DC BOTH: Decompounding word %s into %s and %s", compoundWord, max->prefix, max->suffix);
		}
		else if (max->prefixConwt.con > 0) {
			if (UNDEF == doc_desc.ctypes[0].con_to_token->proc (&(max->prefixConwt.con), &max->prefix, contok_inst))
				return UNDEF;
			snprintf(msg, sizeof(msg), "DC P: Decompounding word %s into %s", compoundWord, max->prefix);
		}
		else if (max->suffixConwt.con > 0) {
			if (UNDEF == doc_desc.ctypes[0].con_to_token->proc (&(max->suffixConwt.con), &max->suffix, contok_inst))
				return UNDEF;
			snprintf(msg, sizeof(msg), "DC S:Decompounding word %s into %s", compoundWord, max->suffix);
		}
    	PRINT_TRACE (4, print_string, msg);
	}

	// Insert the new compound(s) in the conwtp buffer
	if (max->prefixConwt.con > 0) {
		if ((corr = insertDecompound(ip, conwtp, &(max->prefixConwt), alpha)) <= 0) {
			snprintf(msg, sizeof(msg), "DC CORR: Skipping decomposition of %s (corr = %f)", compoundWord, corr);
    		PRINT_TRACE (2, print_string, msg);
		}
	}
	if (max->suffixConwt.con > 0) {
		if ((corr = insertDecompound(ip, conwtp, &(max->suffixConwt), beta)) <= 0) {
			snprintf(msg, sizeof(msg), "DC CORR: Skipping decomposition %s (corr = %f)", compoundWord, corr);
    		PRINT_TRACE (2, print_string, msg);
		}
	}
	return 0;	
}

static int getNumIntersections(INV* inv1, INV* inv2) {
	LISTWT *lwp1 = inv1->listwt, *lwp2 = inv2->listwt;
	int co_df = 0;	
    /* Find common docs. that are in top-retrieved set */
    while (lwp1 < inv1->listwt + inv1->num_listwt &&
		   lwp2 < inv2->listwt + inv2->num_listwt) {
		if (lwp1->list < lwp2->list) lwp1++;
		else if (lwp1->list > lwp2->list) lwp2++;
		else {		    
		    co_df++;
		    lwp1++; lwp2++;
		}
	}
	return co_df;
}

// Insert the new decompound in its proper place (sorted by cons)
// in the output conwt buffer.
// Adjust the weight of this decompound by the Jacard Coeff.
static float insertDecompound(STATIC_INFO* ip, CON_WT* compound, CON_WT* decompound, float wt) {
	INV inv_c, inv_dc; 
	CON_WT *found_conwt, *conwt, *next_conwt;
	int intersections;
	float corr;
	long N = num_docs;

	if (corr_thresh > 0) {
		inv_c.id_num = compound->con;
		if (1 != seek_inv(inv_fd, &inv_c) ||
			1 != read_inv(inv_fd, &inv_c)) {
			snprintf(msg, sizeof(msg), "compound con %d not found in inv file", compound->con);
			return 0;
		}

   		inv_dc.id_num = decompound->con;
		if (1 != seek_inv(inv_fd, &inv_dc) ||
			1 != read_inv(inv_fd, &inv_dc)) {
			///snprintf(msg, sizeof(msg), "decompound con %d not found in inv file", decompound->con);
			///print_error("insertDecompound", msg);
			return 0;
		}
		intersections = getNumIntersections(&inv_c, &inv_dc);

		corr = intersections / (float)MIN(inv_c.num_listwt, inv_dc.num_listwt);
		PRINT_TRACE(4, print_float, &corr);

		// Insert this decompound only if the corr > threshold
		if (corr < corr_thresh) {
			return corr;
		}
	}
	
	found_conwt = (CON_WT*) searchCon(decompound, ip->conwt_buff, ip->num_words);
	if (found_conwt->con == decompound->con) {
		found_conwt->wt += wt * decompound->wt;
		return 1;
	}

	// realloc buffer if necessary
	if (ip->num_words+1 >= ip->conwt_buff_size) {
		ip->conwt_buff_size = twice(ip->num_words+1);
		ip->conwt_buff = Realloc(ip->conwt_buff, ip->conwt_buff_size, CON_WT);
	}

	// shift the rest of the array downwards
	for (conwt = &ip->conwt_buff[ip->num_words-1]; conwt >= found_conwt; conwt--) {
		next_conwt = conwt + 1;
		next_conwt->con = conwt->con;
		next_conwt->wt = conwt->wt;
	}
	conwt++;
	conwt->con = decompound->con;
	conwt->wt = wt*decompound->wt;
	ip->num_words++;
	return 1;
}

int vecexp_decomp (VEC* invec, VEC* ovec, int inst)
{
	long    i, j;
	CON_WT  *conwtp;
	long    nonword_len, num_conwt;
	STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering vecexp_decomp");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "vecexp_decomp");
        return (UNDEF);
    }
	ip = &info[inst];

	// copy all words into the ovec buffer
	ip->num_words = invec->ctype_len[0];
	if (ip->num_words >= ip->conwt_buff_size) {
		ip->conwt_buff_size = twice(ip->num_words);
		ip->conwt_buff = Realloc(ip->conwt_buff, ip->conwt_buff_size, CON_WT);
	}
	memcpy(ip->conwt_buff, invec->con_wtp, sizeof(CON_WT)*ip->num_words);

	// Iterate through all the document words...
	for (conwtp = invec->con_wtp; conwtp < &invec->con_wtp[invec->ctype_len[0]]; conwtp++) {
		// Now fill up the compounds, if there are any...
		if (UNDEF == expandWordWithCompounds(ip, conwtp)) 
			return UNDEF;
	}

	// Copy the rest of the ctypes as-is from invec onto the ovec
	nonword_len = invec->num_conwt - invec->ctype_len[0];
	num_conwt = ip->num_words + nonword_len;
	if (num_conwt >= ip->conwt_buff_size) {
		ip->conwt_buff_size = twice(num_conwt);
		ip->conwt_buff = Realloc(ip->conwt_buff, ip->conwt_buff_size, CON_WT);
	}
	memcpy(ip->conwt_buff + ip->num_words, invec->con_wtp + invec->ctype_len[0], sizeof(CON_WT)*nonword_len);

	// Copy the ctype_len buffer of the input vec onto the output vec
	if (invec->num_ctype >= ip->ctype_len_buff_size) {
		ip->ctype_len_buff_size = twice(invec->num_ctype);
		ip->ctype_len_buff = Realloc(ip->ctype_len_buff, ip->ctype_len_buff_size, long);
	}
	memcpy(ip->ctype_len_buff, invec->ctype_len, sizeof(long)*invec->num_ctype);

	ovec->id_num.id = invec->id_num.id;
	ovec->num_ctype = invec->num_ctype;
	ovec->ctype_len = ip->ctype_len_buff;
	ovec->ctype_len[0] = ip->num_words;
	ovec->num_conwt = num_conwt;
	ovec->con_wtp = ip->conwt_buff;

    PRINT_TRACE (2, print_string, "Trace: leaving vecexp_decomp");
    return (1);
}


int close_vecexp_decomp (int inst)
{
	STATIC_INFO *ip;
    PRINT_TRACE (2, print_string, "Trace: entering close_vecexp_decomp");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "vecexp_decomp");
        return (UNDEF);
    }
	ip = &info[inst];

    if (UNDEF == doc_desc.ctypes[0].con_to_token->close_proc(contok_inst))
        return (UNDEF);

	close_dict(dict_fd);

	// Free the frequency array.
	FREE(freq);

    if (UNDEF == close_dir_array (collstats_fd))
	    return (UNDEF);
    if (UNDEF == close_inv(inv_fd))
		return (UNDEF);

	FREE(ip->conwt_buff);
	FREE(ip->ctype_len_buff);

	ip->valid_info = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving close_vecexp_decomp");
    return (0);
}

