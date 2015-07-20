#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/synt_phrase.c,v 11.2 1993/02/03 16:49:43 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 
 *1
 *2 text_synphr_obj (text_file, vec_file, inst)
 *3   char *text_file;
 *3   char *vec_file;
 *3   int inst;

 *4 init_text_synphr_obj (spec, unused)
 *5   "text_synphr.print"
 *5   "text_synphr.trace"
 *4 close_text_synphr_obj (inst)

 *7 Only 2 word phrases and 2 word sub-phrases of longer phrases are added.
 *9 Caution: "!" is used by the lisp postprocessing program to mark the 
 *9 end of a document . If the normal output contains "!" we will be in 
 *9 trouble.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "context.h"
#include "proc.h"
#include "sm_display.h"
#include "docindex.h"
#include "inst.h"
#include "io.h"
#include "vector.h"
#include "inv.h"
#include <ctype.h>
#include <string.h>
#include <unistd.h>

static char *ip_list_file, *default_vec_file;
static long vec_mode;
static PROC_TAB *contok, *tokcon;
static long dict_flag, stem_flag, q_flag;

static SPEC_PARAM spec_args[] = {
    {"text_synphr.input_list_file", getspec_dbfile, (char *)&ip_list_file},
    {"text_synphr.vec_file", getspec_dbfile, (char *)&default_vec_file},
    {"text_synphr.vec_file.rwmode", getspec_filemode, (char *)&vec_mode},
    {"text_synphr.con_to_token", getspec_func, (char *)&contok},
    {"text_synphr.token_to_con", getspec_func, (char *)&tokcon},
    {"text_synphr.add_dict", getspec_bool, (char *)&dict_flag},
    {"text_synphr.stem_wanted", getspec_bool, (char *)&stem_flag},
    {"text_synphr.query_flag", getspec_bool, (char *)&q_flag},
    TRACE_PARAM ("text_synphr.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static long sp_inst, contok_inst, w_inst, vec_inst;
static int (*add_phrase)(), (*get_vec)();
static char **words;
static int max_words;
static CON_WT *phrases;
static int max_phr;
static VEC merged_vec;
static int max_conwt;
static int add_phrase_ss(), add_phrase_noss(), merge_vecs(), compare_con();

int init_tokcon_dict_niss(), tokcon_dict_niss(), close_tokcon_dict_niss();


int
init_text_synphr_obj (spec, unused)
SPEC *spec;
char *unused;
{
    CONTEXT old_context;

    PRINT_TRACE (2, print_string, "Trace: entering init_text_synphr_obj");

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
	return (UNDEF);

    old_context = get_context();
    if (dict_flag) set_context (CTXT_INDEXING_DOC);
    if (UNDEF == (sp_inst = init_tokcon_dict_niss(spec, "synphr.")))
	return(UNDEF);
    set_context (old_context);
    if (UNDEF == (contok_inst = contok->init_proc(spec, "ctype.0.")) ||
	UNDEF == (w_inst = tokcon->init_proc(spec, "index.section.word.")))
	return(UNDEF);

    /* Won't need to do this once qid_vec and did_vec are unified through
     * the use of prefixes. */
    if (q_flag) {
	if (UNDEF == (vec_inst = init_qid_vec(spec, (char *) NULL)))
	    return(UNDEF);
	get_vec = qid_vec;
    }
    else {
	if (UNDEF == (vec_inst = init_did_vec(spec, (char *) NULL)))
	    return(UNDEF);
	get_vec = did_vec;
    }
    if (stem_flag) add_phrase = add_phrase_ss;
    else add_phrase = add_phrase_noss;

    words = NULL; max_words = 0;
    phrases = NULL; max_phr = 0;
    max_conwt = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving init_text_synphr_obj");
    return 0;
}

int
text_synphr_obj (text_file, vec_file, inst)
char *text_file;
char *vec_file;
int inst;
{
    char namebuf[PATH_LEN], inbuf[PATH_LEN], *token, in_doc;
    int vec_fd;
    long ctypelen[3], line_len, did, num_phr, tf, i, j;
    FILE *fp, *list_fp;
    VEC tmp_vec;

    PRINT_TRACE (2, print_string, "Trace: entering text_synphr_obj");

    /* Open files */
    if (VALID_FILE (text_file)) {
        if (NULL == (list_fp = fopen(text_file, "r"))) {
	    set_error(SM_ILLPA_ERR, text_file, "text_synphr_obj");
            return (UNDEF);
	}
    }
    else if (VALID_FILE(ip_list_file)) {
	if (NULL == (list_fp = fopen(ip_list_file, "r"))) {
	    set_error(SM_ILLPA_ERR, ip_list_file, "text_synphr_obj");
	    return(UNDEF);
	}
    }    
    else list_fp = stdin;
    if (vec_file == (char *) NULL)
        vec_file = default_vec_file;
    if (UNDEF == (vec_fd = open_vector(vec_file, vec_mode))) {
        clr_err();
        if (UNDEF == (vec_fd = open_vector(vec_file, vec_mode | SCREATE)))
            return (UNDEF);
    }

    merged_vec.num_ctype = 3;
    merged_vec.ctype_len = ctypelen;

    while (NULL != fgets(namebuf, PATH_LEN, list_fp)) {
	namebuf[strlen(namebuf) - 1] = '\0';
	if (NULL == (fp = fopen(namebuf, "r"))) {
	    set_error(SM_ILLPA_ERR, namebuf, "text_synphr_obj");
	    return(UNDEF);
	}
	in_doc = 0; num_phr = 0;
	/* Process each line in turn */
	while (NULL != fgets(inbuf, PATH_LEN, fp)) {
	    line_len = strlen(inbuf) - 1;
	    if (line_len == 0) continue;
	    switch (inbuf[0]) {
	        case '1': case '2': case '3': case '4': case '5': 
	        case '6': case '7': case '8': case '9':
		    /* Start of a new document */
		    if (in_doc) {
			fprintf(stderr, "%s: Unexpected line %s\n", 
				namebuf, inbuf);
			break;
		    }
		    sscanf(inbuf, "%ld", &did);
		    in_doc = 1; num_phr = 0;
		    break;
	        case '!': 
		    /* End of current document: generate the final weighted 
		     * vector (see note for add_phrase) and write it out. */
		    if (!in_doc) {
			fprintf(stderr, "%s: Unexpected line %s\n", 
				namebuf, inbuf);
			break;
		    }
		    if (UNDEF == get_vec(&did, &tmp_vec, vec_inst))
			return(UNDEF);
		    if (num_phr > 0) {
			qsort(phrases, num_phr, sizeof(CON_WT), compare_con);
			tf = 1;
			for (i = 0, j = 1; j < num_phr; j++) {
			    if (phrases[j].con == phrases[i].con)
				tf++;
			    else {
				phrases[i].wt = tf;
				phrases[++i] = phrases[j];
				tf = 1; 
			    }
			}
			phrases[i].wt = tf; 
			num_phr = i + 1;
		    }
		    merged_vec.id_num.id = did; EXT_NONE(merged_vec.id_num.ext);
		    if (UNDEF == merge_vecs(&tmp_vec, phrases, num_phr, 
					    &merged_vec))
			return(UNDEF);
		    if (UNDEF == seek_vector (vec_fd, &merged_vec) ||
			1 != write_vector (vec_fd, &merged_vec))
			return (UNDEF);
		    in_doc = 0;
		    break;
	        case '(': /* Handle this phrase */
		    if (!in_doc || inbuf[line_len - 1] != ')') {
			fprintf(stderr, "%s: Unexpected line %s\n", 
				namebuf, inbuf);
			break;
		    }
		    token = inbuf + 1;
		    inbuf[line_len - 1] = '\0';
		    if (UNDEF == (*add_phrase)(token, &num_phr))
			return(UNDEF);
		    break;
	        case '>': /* Either comment or docid or blank */
		    if (in_doc || inbuf[1] != ' ' ||
			(line_len > 2 && inbuf[2] != ';' && !isdigit(inbuf[2])))
			fprintf(stderr, "%s: Unexpected line %s\n", 
				namebuf, inbuf);
		    if (isdigit(inbuf[2])) {
			/* Start of a new document */
			sscanf(inbuf + 2, "%ld", &did);
			in_doc = 1; num_phr = 0;
		    }
		    break;
	        case ';': /* Comment line */
		    if (in_doc) 
			fprintf(stderr, "%s: Unexpected line %s\n", 
				namebuf, inbuf);
		    break;
	        case 'N': /* NIL */
		    if (in_doc || line_len != 3 ||
			inbuf[1] != 'I' || inbuf[2] != 'L')
			fprintf(stderr, "%s: Unexpected line %s\n", 
				namebuf, inbuf);
		    break;
	        case '#': /* Sanity check */
		    if (in_doc || inbuf[1] != 'P')
			fprintf(stderr, "%s: Unexpected line %s\n", 
				namebuf, inbuf);
		    break;
	        case '-': /* Sanity check */
		    if (in_doc || inbuf[1] != '>')
			fprintf(stderr, "%s: Unexpected line %s\n", 
				namebuf, inbuf);
		    break;
	        default:
		    fprintf(stderr, "%s: Unexpected line %s\n", 
			    namebuf, inbuf);
		    break;
	    }
	}
	fclose(fp);
    }

    if (VALID_FILE (text_file) || VALID_FILE(ip_list_file))
        (void) fclose(list_fp);
    if (UNDEF == close_vector(vec_fd))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving text_synphr_obj");
    return 0;
}

int
close_text_synphr_obj (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_text_synphr_obj");

    if (UNDEF == close_tokcon_dict_niss(sp_inst) ||
	UNDEF == contok->close_proc(contok_inst) ||
	UNDEF == tokcon->close_proc(w_inst))
	return(UNDEF);

    if (q_flag) {
	if (UNDEF == close_qid_vec(vec_inst))
	    return(UNDEF);
    }
    else {
	if (UNDEF == close_did_vec(vec_inst))
	    return(UNDEF);
    }

    if (max_words) Free(words);
    if (max_phr) Free(phrases);
    if (max_conwt) Free(merged_vec.con_wtp);

    PRINT_TRACE (2, print_string, "Trace: leaving close_text_synphr_obj");
    return (0);
}


/* Add the phrase represented by tok and phrases formed by taking each 
 * pair of words in tok to the syntactic phrase dictionary.
 * Assumes that "tok" is in lisp output format (sans the brackets), i.e.,
 * it consists of a +ve no. of single words separated by single blanks. 
 * For example: "LONG TERM CARE CONFINEMENTS".
 */
int
add_phrase_noss(tok, num)
char *tok;
long *num;
{
    char tok_buf[PATH_LEN], *cptr;
    long num_words, num_p, sp_con, i, j;

    /* Count the number of single words */
    for (cptr = tok, num_words = 1; *cptr; cptr++) {
	*cptr = (char) tolower(*cptr);
	if (*cptr == ' ') num_words++;
    }
    if (num_words == 1) return(0);

    /* Allocate space if needed */
    if (num_words > max_words) {
	if (max_words) Free(words);
	max_words += 2 * num_words;
	if (NULL == (words = Malloc(max_words, char *)))
	    return(UNDEF);
    }
    if (*num + (num_words * (num_words - 1))/2 + 1 > max_phr) {
	max_phr += num_words * (num_words - 1) + 1;
	if (NULL == (phrases = Realloc(phrases, max_phr, CON_WT)))
	    return(UNDEF);
    }

    /* Get token for each word */
    for (i = 0, words[0] = cptr = tok; i < num_words-1; cptr++) {
	if (*cptr == ' ') {
	    *cptr = '\0';
	    words[++i] = cptr + 1;
	}
    }

    /* Add each pair of words as a phrase */
    num_p = *num;
    for (i = 0; i < num_words; i++) {
	for (j = i + 1; j < num_words; j++) {
	    sprintf(tok_buf, "%s %s", words[i], words[j]);
	    if (UNDEF == tokcon_dict_niss(tok_buf, &sp_con, sp_inst))
		return(UNDEF);
	    if (UNDEF == sp_con) continue;
	    phrases[num_p].con = sp_con;
	    phrases[num_p++].wt = 1.0;
	}
    }

    /* Add entire phrase */
    if (num_words > 2) {
	for (i = 0, cptr = tok; i < num_words - 1; cptr++) 
	    if (*cptr == '\0') {
		*cptr = ' ';
		i++;
	    }
	if (UNDEF == tokcon_dict_niss(tok, &sp_con, sp_inst))
	    return(UNDEF);
	if (UNDEF != sp_con) {
	    phrases[num_p].con = sp_con;
	    phrases[num_p++].wt = 1.0;
	}
    }

    *num = num_p;
    return 1;
}

/* As above, except that all stopwords are removed from tok and 
 * the remaining words are replaced by their stems.
 */
int
add_phrase_ss(tok, num)
char *tok;
long *num;
{
    char stem_buf[PATH_LEN], tok_buf[PATH_LEN], *stemmed_tok, *cptr;
    long num_words, num_p, w_con, sp_con, i, j;

    /* Count the number of single words */
    for (cptr = tok, num_words = 1; *cptr; cptr++) {
	*cptr = (char) tolower(*cptr);
	if (*cptr == ' ') num_words++;
    }
    if (num_words == 1) return(0);

    /* Allocate space if needed */
    if (num_words > max_words) {
	if (max_words) Free(words);
	max_words += 2 * num_words;
	if (NULL == (words = Malloc(max_words, char *)))
	    return(UNDEF);
    }

    /* Get token for each word and stop/stem it */
    for (i = 0, words[0] = cptr = tok; i < num_words-1; cptr++) {
	if (*cptr == ' ') {
	    *cptr = '\0';
	    words[++i] = cptr + 1;
	}
    }
    for (i = 0, j = 0, cptr = stem_buf; i < num_words; i++) {
	if (UNDEF == tokcon->proc(words[i], &w_con, w_inst))
	    return(UNDEF);
	if (UNDEF == w_con) continue;
	if (UNDEF == contok->proc(&w_con, &stemmed_tok, contok_inst))
	    return(UNDEF);
	strcpy(cptr, stemmed_tok); 
	words[j] = cptr; cptr += strlen(stemmed_tok) + 1;
	j++;
    }
    if (j < 2) return(0);
    num_words = j;

    /* Allocate space if needed */
    if (*num + (num_words * (num_words - 1))/2 + 1 > max_phr) {
	max_phr += num_words * (num_words - 1) + 1;
	if (NULL == (phrases = Realloc(phrases, max_phr, CON_WT)))
	    return(UNDEF);
    }

    /* Add each pair of words as a phrase */
    num_p = *num;
    for (i = 0; i < num_words; i++) {
	for (j = i + 1; j < num_words; j++) {
	    sprintf(tok_buf, "%s %s", words[i], words[j]);
	    if (UNDEF == tokcon_dict_niss(tok_buf, &sp_con, sp_inst))
		return(UNDEF);
	    if (UNDEF == sp_con) continue;
	    phrases[num_p].con = sp_con;
	    phrases[num_p++].wt = 1.0;
	}
    }

    /* Add entire phrase */
    if (num_words > 2) {
	for (i = 0, cptr = tok_buf; i < num_words; i++, cptr++) {
	    sprintf(cptr, "%s", words[i]);
	    cptr += strlen(words[i]);
	    *cptr = ' ';
	}
	*(cptr-1) = '\0';
	if (UNDEF == tokcon_dict_niss(tok_buf, &sp_con, sp_inst))
	    return(UNDEF);
	if (UNDEF != sp_con) { 
	    phrases[num_p].con = sp_con;
	    phrases[num_p++].wt = 1.0;
	}
    }

    *num = num_p;
    return 1;
}

static int
merge_vecs(v1, phrvec, numphr, mvec)
VEC *v1;
CON_WT *phrvec;
long numphr;
VEC *mvec;
{
    long i;

    assert(v1->num_ctype == 2);
    mvec->ctype_len[0] = v1->ctype_len[0];
    mvec->ctype_len[1] = v1->ctype_len[1];
    mvec->ctype_len[2] = numphr;
    mvec->num_conwt = v1->num_conwt + numphr;

    if (v1->num_conwt + numphr > max_conwt) {
	if (max_conwt) Free(mvec->con_wtp);
	max_conwt += v1->num_conwt + numphr;
	if (NULL == (mvec->con_wtp = Malloc(max_conwt, CON_WT)))
	    return(UNDEF);
    }

    for (i = 0; i < v1->num_conwt; i++)
	mvec->con_wtp[i] = v1->con_wtp[i];
    for (i = 0; i < numphr; i++)
	mvec->con_wtp[v1->num_conwt + i] = phrvec[i];

    return 1;
}


static int
compare_con(c1, c2)
CON_WT *c1, *c2;
{
    return(c1->con - c2->con);
}
