#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libevaluate/rrvec_trec_eval.c,v 11.0 1992/07/21 18:20:34 chrisb Exp chrisb $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 
 * Runs LDA inference for all documents in the collection to build
 * up the in-memory matrices theta (MxK) and phi (KxV). This
 * is built by reading in the nnn_doc_file.
 * Then use these matrices to compute the marginalized 
 *
 *
 *
   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "io.h"
#include "rel_header.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "eval.h"
#include "inst.h"
#include "dir_array.h"
#include "vector.h"
#include "context.h"
#include "rr_vec.h"
#include "tr_vec.h"
#include "collstat.h"
#include "lda.h"

static int doc_fd, nnn_doc_fd;
static char* dvec_file, *nnn_dvec_file ;
static char *textloc_file;      /* Used to tell how many docs in collection */
static long dvec_file_mode;

static int collstats_fd;
static char* collstat_file ;
static long collstat_mode ;
static long collstats_num_freq;		// total number of unique terms in collection
static long median_doc_len;
static float alpha, beta;
static int multifaceted;

typedef struct {
    int valid_info;
	CON_WT *conwt_buff;
	CON_WT *compact_conwt_buff;
	int conwt_buff_size;
	long *ctype_len_buff;
	int   ctype_len_buff_size;
	long  max_did;
	int   min_did;
	LDAModel ldamodel;
}
STATIC_INFO;
static STATIC_INFO *info;

static int max_inst = 0;
static int print_vec_inst;
static float* totalLMWtsPerTopic;

static long getDocumentLength(VEC* vec, int ctype);
static int numDocsPrinted = 0;

static SPEC_PARAM spec_args[] = {
    {"doc_exp.alpha",                getspec_float,   (char *) &alpha},
    {"doc_exp.beta",                 getspec_float,    (char *) &beta},
    {"doc_exp.doc_file.rmode",       getspec_filemode, (char *) &dvec_file_mode},
    {"doc_exp.collstat_file",        getspec_dbfile,   (char *) &collstat_file},
    {"doc_exp.collstat_file.rmode",  getspec_filemode, (char *) &collstat_mode},
    {"doc.textloc_file",             getspec_dbfile,   (char *) &textloc_file},
    {"doc_exp.median_doclen",        getspec_long,     (char *) &median_doc_len},
    {"docexp_trlm.nnn_doc_file",         getspec_dbfile,   (char *) &nnn_dvec_file},
    {"docexp_trlm.doc_file",             getspec_dbfile,   (char *) &dvec_file},
    {"docexp_trlm.multifaceted",     getspec_bool,     (char *) &multifaceted},
    TRACE_PARAM ("docexp_trlm.trace")
};

static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

int
init_docexp_trlm_coll (spec, unused)
SPEC *spec;
char *unused;
{
	DIR_ARRAY dir_array;
	int new_inst;
	STATIC_INFO *ip;
    REL_HEADER *rh;

    PRINT_TRACE (2, print_string, "Trace: entering/leaving init_docexp_trlm_coll");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
	ip = &info[new_inst];
    ip->valid_info = 1;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    /* Open the document vector file (direct file) */
    if (! VALID_FILE (dvec_file))
		return UNDEF;
   	if (UNDEF == (doc_fd = open_vector (dvec_file, dvec_file_mode)))
       	return (UNDEF);
    if (! VALID_FILE (nnn_dvec_file))
		return UNDEF;
   	if (UNDEF == (nnn_doc_fd = open_vector (nnn_dvec_file, dvec_file_mode)))
       	return (UNDEF);

	if (! VALID_FILE (collstat_file)) {
		return UNDEF;
    }
    else {
		if (UNDEF == (collstats_fd = open_dir_array (collstat_file, collstat_mode)))
			return (UNDEF);

		// Read in collection frequencies
        dir_array.id_num = COLLSTAT_COLLFREQ; // Get the document frequency list from the file
        if (1 != seek_dir_array (collstats_fd, &dir_array) ||
            1 != read_dir_array (collstats_fd, &dir_array)) {
            collstats_num_freq = 0;
			return UNDEF;
        }
        else {
            // Read from file successful. Allocate 'freq' array and dump the
            // contents of the file in this list
            collstats_num_freq = dir_array.num_list / sizeof (float);
        }
    }

	ip->conwt_buff = Malloc(collstats_num_freq, CON_WT);
	if (!ip->conwt_buff)
		return UNDEF;
	ip->ctype_len_buff_size = 16;
	ip->ctype_len_buff = Malloc(ip->ctype_len_buff_size, long);
	if (!ip->ctype_len_buff)
		return UNDEF;

	ip->conwt_buff_size = 1024;
	if (NULL == (ip->compact_conwt_buff = Malloc(ip->conwt_buff_size, CON_WT)))
		return UNDEF;

	// Get the max doc id.
	if (NULL == (rh = get_rel_header (textloc_file)) || !rh->max_primary_value)
		return UNDEF;
	ip->max_did = rh->max_primary_value;

	if (sm_trace >= 8 && UNDEF == (print_vec_inst = init_print_vec_dict(spec, NULL)))
		return UNDEF;

	// initalize the LM conversion module
	if (UNDEF == init_lang_model_wt_lm(spec, NULL))
		return UNDEF;

	/* 	Call the LDA estimator to estimate P(w|z) and P(z|d)s over the whole collection */
	if (UNDEF == init_lda_est(&ip->ldamodel, spec)) {
		return UNDEF;
	}
	if (UNDEF == init_lda_est_inst_coll(&ip->ldamodel, nnn_doc_fd, ip->max_did)) {
		fprintf(stderr, "Unable to initialize LDA estimation for the whole collection\n");
		return UNDEF;
	}
	lda_est(&ip->ldamodel);
	totalLMWtsPerTopic = (float*) malloc (sizeof(float) * ip->ldamodel.K);

    PRINT_TRACE (2, print_string, "Trace: leaving init_docexp_trlm_coll");
    return (new_inst);
}

static int comp_did(TR_TUP* this, TR_TUP* that) {
	return this->did < that->did? -1 : this->did == that->did? 0 : 1;
}

static int expand_from_adjacent(TR_VEC* tr_vec, STATIC_INFO* ip, int delta, float beta) {
	VEC dvec;
	float doclen;
	CON_WT* conwtp;
	TR_TUP key, *found;

	dvec.id_num.id = tr_vec->qid + delta;
	if (!(dvec.id_num.id >= ip->min_did && dvec.id_num.id <= ip->max_did))
		return 0;

	// Don't expand if it was already retrieved in the neighborhood
	key.did = dvec.id_num.id;
	if (NULL != (found = bsearch(&key, tr_vec->tr, tr_vec->num_tr, sizeof(TR_TUP), comp_did)))
		return 0;

	if (UNDEF == seek_vector (doc_fd, &dvec) ||
    	UNDEF == read_vector (doc_fd, &dvec))
   	   	return UNDEF;

	doclen = (float)getDocumentLength(&dvec, 0);
	for (conwtp = dvec.con_wtp; conwtp < &dvec.con_wtp[dvec.ctype_len[0]]; conwtp++) {
		ip->conwt_buff[conwtp->con].con = conwtp->con; 
		ip->conwt_buff[conwtp->con].wt += beta*conwtp->wt/doclen; 
	}
	return 1;
}

int docexp_trlm_coll (VEC* invec, VEC* ovec, TR_VEC* tr_vec, int inst)
{
	TR_TUP      *tr_tup;
	long        i, j;
	VEC         dvec, invec_lm, *pvec;
	CON_WT      *conwtp;
	long        num_words, nonword_len, num_conwt;
	STATIC_INFO *ip;
	long        doclen;
	float       p_D_Q, p_w_D;
	int         k, topicId, wordId;

    PRINT_TRACE (2, print_string, "Trace: entering docexp_trlm_coll");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "docexp_trlm_coll");
        return (UNDEF);
    }
	ip = &info[inst];

	doclen = getDocumentLength(invec, 0);
	if (doclen < median_doc_len) { 
    	PRINT_TRACE (2, print_string, "Trace: skipping expansion");
    	if (UNDEF == copy_vec (ovec, invec))
        	return (UNDEF);
		return 0;
	}

	PRINT_TRACE (4, print_string, "Current doc: ");
	PRINT_TRACE (4, print_vector, invec);
    if (sm_trace >= 8) {
		PRINT_TRACE (8, print_string, "Current doc: ");
		if (UNDEF == print_vec_dict(invec, NULL, print_vec_inst))
			return UNDEF;
	}

	if (ip->min_did == 0) {
		ip->min_did = invec->id_num.id; // save the minimum doc_id
	}
 
	// Reset the buffer to zeroes
	memset(ip->conwt_buff, 0, sizeof(CON_WT)*collstats_num_freq);

	/* Convert the raw term frequency vectors to LM weights */
	if (UNDEF == lang_model_wt_lm(invec, &invec_lm, 0))
		return UNDEF;
	pvec = &invec_lm;

	// Initialize the conwt_buff array to the term frequency values from this document
	for (conwtp = pvec->con_wtp; conwtp < &pvec->con_wtp[pvec->ctype_len[0]]; conwtp++) {
		ip->conwt_buff[conwtp->con].con = conwtp->con; 
		ip->conwt_buff[conwtp->con].wt = alpha*conwtp->wt; 
	}

	// Fill up the contributions from neighboring documents
	for (i = 0; i < tr_vec->num_tr; i++) {
		tr_tup = &tr_vec->tr[i];

		if (tr_tup->did == pvec->id_num.id)
			continue;	// Don't expand with itself

	    dvec.id_num.id = tr_tup->did;
    	if (UNDEF == seek_vector (doc_fd, &dvec) ||
   	    	UNDEF == read_vector (doc_fd, &dvec)) {
   	    	return UNDEF;
	    }
		PRINT_TRACE (4, print_string, "Neighborhood doc: ");
		PRINT_TRACE (4, print_vector, &dvec);
    	if (sm_trace >= 8) {
			PRINT_TRACE (8, print_string, "Neighborhood doc: ");
			if (UNDEF == print_vec_dict(&dvec, NULL, print_vec_inst))
				return UNDEF;
		}

		doclen = getDocumentLength(&dvec, 0);
		p_D_Q = !multifaceted? tr_tup->sim : compute_marginalized_P_w_D(&ip->ldamodel, invec->con_wtp, invec->id_num.id-1);

		// Iterate for every word in this document
		for (conwtp = dvec.con_wtp; conwtp < &dvec.con_wtp[dvec.ctype_len[0]]; conwtp++) {
			ip->conwt_buff[conwtp->con].con = conwtp->con; 
			p_w_D = compute_marginalized_P_w_D(&ip->ldamodel, conwtp, tr_tup->did-1);
			if (ip->conwt_buff[conwtp->con].wt == 0) {
				// a new expansion term is being added. note its topic id
				// and increment the counter
				wordId = getWordMap(&(ip->ldamodel), conwtp);
				if (wordId >=0 && wordId < ip->ldamodel.docs[dvec.id_num.id-1].length) {
					topicId = ip->ldamodel.z[dvec.id_num.id-1][wordId];
					totalLMWtsPerTopic[topicId]++;
				}
			}
			ip->conwt_buff[conwtp->con].wt += (1-alpha-beta)*p_w_D*p_D_Q;
		}
	}
	
	if (invec->id_num.id == ip->max_did) {
		for (k = 0; k < ip->ldamodel.K; k++) {
			printf("%f ", totalLMWtsPerTopic[k]/(float)ip->max_did);
		}
		printf("\n");
	}

	// Fill up the rest of the contribution from adjacent documents (i.e. documents
	// with id 1 less and 1 more than the current document id).
	if (beta > 0) {
		if (UNDEF == expand_from_adjacent(tr_vec, ip, -1, beta))
			return UNDEF;
		if (UNDEF == expand_from_adjacent(tr_vec, ip, 1, beta))
			return UNDEF;
	}

	// Fill up the output vector
	num_words = 0;
	for (i = 0; i < collstats_num_freq; i++) {
		if (ip->conwt_buff[i].wt > 0) num_words++;
	}
	if (num_words > ip->conwt_buff_size) {
		ip->conwt_buff_size = twice(num_words);
		ip->compact_conwt_buff = Realloc(ip->compact_conwt_buff, ip->conwt_buff_size, CON_WT);
	}

	for (i = 0, j = 0; i < collstats_num_freq; i++) {
		if (ip->conwt_buff[i].wt > 0) {
			ip->compact_conwt_buff[j].con = ip->conwt_buff[i].con;
			ip->compact_conwt_buff[j].wt = ip->conwt_buff[i].wt;
			j++;
		}
	}

	// Copy the rest of the ctypes as-is from invec onto the ovec
	nonword_len = pvec->num_conwt - pvec->ctype_len[0];
	num_conwt = num_words + nonword_len;
	if (num_conwt > ip->conwt_buff_size) {
		ip->conwt_buff_size = twice(num_conwt);
		ip->compact_conwt_buff = Realloc(ip->compact_conwt_buff, ip->conwt_buff_size, CON_WT);
	}
	memcpy(ip->compact_conwt_buff + num_words, pvec->con_wtp + pvec->ctype_len[0], sizeof(CON_WT)*nonword_len);

	// Copy the ctype_len buffer of the input vec onto the output vec
	if (pvec->num_ctype > ip->ctype_len_buff_size) {
		ip->ctype_len_buff_size = twice(pvec->num_ctype);
		ip->ctype_len_buff = Realloc(ip->ctype_len_buff, ip->ctype_len_buff_size, long);
	}
	memcpy(ip->ctype_len_buff, pvec->ctype_len, sizeof(long)*pvec->num_ctype);

	ovec->id_num.id = pvec->id_num.id;
	ovec->num_ctype = pvec->num_ctype;
	ovec->ctype_len = ip->ctype_len_buff;
	ovec->ctype_len[0] = num_words;
	ovec->num_conwt = num_conwt;
	ovec->con_wtp = ip->compact_conwt_buff;


    PRINT_TRACE (2, print_string, "Trace: leaving docexp_trlm_coll");
    return (1);
}

int close_docexp_trlm_coll (int inst)
{
	STATIC_INFO *ip;
    PRINT_TRACE (2, print_string, "Trace: entering close_docexp_trlm_coll");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "docexp_trlm_coll");
        return (UNDEF);
    }
	ip = &info[inst];

	close_lda_est_inst(&ip->ldamodel);
	close_lda_est(&ip->ldamodel);

   	if (UNDEF == close_lang_model_wt_lm(0))
		return UNDEF;
	if ( UNDEF == close_vector(doc_fd) )
		return UNDEF;
	if ( UNDEF == close_vector(nnn_doc_fd) )
		return UNDEF;
    if (UNDEF == close_dir_array (collstats_fd))
	    return (UNDEF);
    if (sm_trace >= 8 && UNDEF == close_print_vec_dict(print_vec_inst))
		return UNDEF;

	FREE(ip->conwt_buff);
	FREE(ip->compact_conwt_buff);
	FREE(ip->ctype_len_buff);
	FREE(totalLMWtsPerTopic);

	ip->valid_info = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving close_docexp_trlm_coll");
    return (0);
}

static long getDocumentLength(VEC* vec, int ctype)
{
	int i;
	long length = 0 ;
	CON_WT* start = vec->con_wtp ;

	// Jump to the appropriate part of the vector
	for (i = 0; i < ctype; i++) {
		start += vec->ctype_len[i] ;
	}

	// Read ctype_len[ctype] weights and add them up.
	for (i = 0; i < vec->ctype_len[ctype]; i++) {
		length += start[i].wt ;
	}

	return length ;
}
