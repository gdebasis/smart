#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libevaluate/rrvec_trec_eval.c,v 11.0 1992/07/21 18:20:34 chrisb Exp chrisb $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Print evaluation results for a set of retrieval runs
 *2 print_obj_eval (in_file, out_file, inst)
 *3   char *in_file;
 *3   char *out_file;
 *3   int inst;
 *4 init_print_obj_eval (spec, unused)
 *4 close_print_obj_eval (inst)
 * Compute the average clarity scores for all queries
***********************************************************************/

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
#include "collstat.h"
#include "dir_array.h"
#include "vector.h"
#include "retrieve.h"

static int tr_fd;
static char* tr_file;
static long tr_mode;
static int q_fd;
static char* qvec_file ;
static long qvec_file_mode;
static int doc_fd;
static char* dvec_file ;
static long dvec_file_mode;
static int collstats_fd;
static char* collstat_file ;
static long collstat_mode ;
static float lambda ;
static long totalDocFreq;
static PROC_INST get_query;

static long num_wanted;				// Number of top docs to consider for clarity evaluation
static long num_terms_wanted;		// Number of unique terms to restrict the computation to

static double *p_w_Q;				// conditional prob. of generating w from the current query Q
									// indexed by the word indices
static float *collstats_freq;		// frequency of the ith term in the collection
static long collstats_num_freq;		// total number of terms in collection
static long collstats_numdocs;		// Total number of documents in the collection

static float getTotalDocumentFreq();
static long getDocumentLength(VEC* vec);
static void computeWordConditionalProbs(VEC* qvec);
static int compare_tr_rank(TR_TUP* t1, TR_TUP* t2);

/* The collstat file is required for iterating over all the terms of the vocabulary
 * while computing the clarity score.
 * The query file is required to compute the product (summation of log) for all
 * the individual terms of Q. We require the tr file to walk through the top R
 * documents and get in the retrieval scores P(D|Q) from it.
 */
static SPEC_PARAM spec_args[] = {
    {"retrieve.get_query",        getspec_func, (char *) &get_query.ptab},
    {"eval_clarity.lambda", getspec_float, (char*)&lambda},
    {"eval_clarity.nnn_doc_file",        getspec_dbfile, (char *) &dvec_file},
    {"eval_clarity.doc_file.rmode",  getspec_filemode, (char *) &dvec_file_mode},
    {"eval_clarity.query_file",        getspec_dbfile, (char *) &qvec_file},
    {"eval_clarity.query_file.rmode",  getspec_filemode, (char *) &qvec_file_mode},
    {"eval_clarity.tr_file",        getspec_dbfile, (char *) &tr_file},
    {"eval_clarity.tr_file.rmode",     getspec_filemode, (char *) &tr_mode},
    {"eval_clarity.collstat_file",        getspec_dbfile, (char *) &collstat_file},
    {"eval_clarity.collstat_file.rmode",  getspec_filemode, (char *) &collstat_mode},
    {"eval_clarity.num_top_docs", getspec_long, (char *) &num_wanted},
    {"eval_clarity.num_terms_wanted", getspec_long, (char *) &num_terms_wanted},
    TRACE_PARAM ("eval_clarity.trace")
};
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);


int
init_proc_eval_clarity_topdocs (spec, unused)
SPEC *spec;
char *unused;
{
	DIR_ARRAY dir_array;

    PRINT_TRACE (2, print_string, "Trace: entering/leaving init_proc_eval_clarity_topdocs");

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    if (UNDEF == (get_query.inst =
                  get_query.ptab->init_proc (spec, NULL)))
		return UNDEF;

    if (! VALID_FILE (dvec_file))
		return UNDEF;
    if (! VALID_FILE (tr_file))	
		return UNDEF;

    /* Open the query vector, LM weighted document vector and the tr files */
   	if (UNDEF == (q_fd = open_vector (qvec_file, qvec_file_mode)))
       	return (UNDEF);

   	if (UNDEF == (doc_fd = open_vector (dvec_file, dvec_file_mode)))
       	return (UNDEF);

    if (UNDEF == (tr_fd = open_tr_vec(tr_file, tr_mode)))
		        return (UNDEF);

	if (! VALID_FILE (collstat_file)) {
		return UNDEF;
    }
    else {
		if (UNDEF == (collstats_fd = open_dir_array (collstat_file, collstat_mode)))
			return (UNDEF);

		// Read the total number of documents
        dir_array.id_num = COLLSTAT_NUMDOC; // Get the collection frequency list from the file
        if (1 != seek_dir_array (collstats_fd, &dir_array) ||
            1 != read_dir_array (collstats_fd, &dir_array)) {
			return UNDEF;
        }
        else {
			memcpy(&collstats_numdocs, dir_array.list, sizeof(long));
        }

		// Read in collection frequencies
        dir_array.id_num = COLLSTAT_TOTWT; // Get the collection frequency list from the file
        if (1 != seek_dir_array (collstats_fd, &dir_array) ||
            1 != read_dir_array (collstats_fd, &dir_array)) {
            collstats_freq = NULL;
            collstats_num_freq = 0;
			return UNDEF;
        }
        else {
            // Read from file successful. Allocate 'freq' array and dump the
            // contents of the file in this list
			collstats_freq = (float*) dir_array.list;
            collstats_num_freq = dir_array.num_list / sizeof (float);
			p_w_Q = (double*) malloc (collstats_num_freq * sizeof(double));
			if (p_w_Q == NULL)
				return UNDEF;
        }

    }

	totalDocFreq = getTotalDocumentFreq();

    PRINT_TRACE (2, print_string, "Trace: leaving init_proc_eval_clarity_topdocs");
    return 1;
}

int
proc_eval_clarity_topdocs (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
    FILE   *output;
	long   i;
	int    status;
	double clarityForThisQuery = 0;
	QUERY_VECTOR query_vec;

    PRINT_TRACE (2, print_string, "Trace: entering proc_eval_clarity_topdocs");

    output = VALID_FILE (out_file) ? fopen (out_file, "w") : stdout;
    if (NULL == output)
        return (UNDEF);

	/* For each query */
    while (1 == (status = get_query.ptab->proc (NULL, &query_vec, get_query.inst))) {
		clarityForThisQuery = 0;
	    if (query_vec.qid < global_start)
		    continue;
		if (query_vec.qid > global_end)
			break;

		// Set the conditional probs. P(w|Q)s to zero
		memset(p_w_Q, 0, collstats_num_freq * sizeof(double));

		computeWordConditionalProbs ((VEC*)(query_vec.vector));

		// Run a loop here to accumulate the clarity score contributions for
		// each word hit thus far.
		for (i = 0; i < collstats_num_freq; i++) {
			if (p_w_Q[i] > 0)
				clarityForThisQuery += p_w_Q[i] * log(p_w_Q[i] * totalDocFreq/collstats_freq[i]); 
		}

		fprintf(output, "clarity(%d) = %f\n", query_vec.qid, clarityForThisQuery);
	}

    PRINT_TRACE (2, print_string, "Trace: leaving proc_eval_clarity_topdocs");
    return (1);
}


int
close_proc_eval_clarity_topdocs (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_proc_eval_clarity_topdocs");

	if ( UNDEF == close_vector(q_fd) )
		return UNDEF ;
	if ( UNDEF == close_vector(doc_fd) )
		return UNDEF;
    if (UNDEF == close_dir_array (collstats_fd))
	    return (UNDEF);
    if (UNDEF == close_tr_vec (tr_fd))
		return UNDEF;

    if (UNDEF == get_query.ptab->close_proc (get_query.inst))
		return UNDEF;

	FREE(p_w_Q);

    PRINT_TRACE (2, print_string, "Trace: leaving close_proc_eval_clarity_topdocs");
    return (0);
}

static float getTotalDocumentFreq()
{
	long i;
	float totalDocFreq = 0 ;
	for (i = 0; i < collstats_num_freq; i++) {
		totalDocFreq += collstats_freq[i] ;
	}
	return totalDocFreq ;
}

static long getDocumentLength(VEC* vec)
{
	long i, length = 0 ;
	CON_WT* start = vec->con_wtp ;

	// Read ctype_len[ctype] weights and add them up.
	for (i = 0; i < vec->ctype_len[0]; i++) {
		length += start[i].wt ;
	}
	return length ;
}

static int compare_tr_rank(t1, t2)
TR_TUP *t1;
TR_TUP *t2;
{
    return t1->rank < t2->rank? -1 : 1;
}

/* Accumulate the probabilities P(w|Q) for every word w
 * encountered while reading in the top documents. */
static void computeWordConditionalProbs(VEC* qvec)
{
	TR_VEC  tr;
	long    tr_ind;
	VEC     dvec;
	CON_WT  *conwtp, key_conwt;
	double  p_w_D, p_D_Q, clarity = 0;
	double  docLen;
	TR_TUP* otr_tup; // ordered by rank
	int     termCount = 0;

    tr.qid = qvec->id_num.id;
    if (UNDEF == seek_tr_vec(tr_fd, &tr) ||
		UNDEF == read_tr_vec(tr_fd, &tr))
		return(UNDEF);

	otr_tup = (TR_TUP*)malloc (sizeof(TR_TUP) * tr.num_tr);
	if (otr_tup == NULL)
		return UNDEF;
	memcpy(otr_tup, tr.tr, tr.num_tr * sizeof(TR_TUP));
	// Sort the tr vector by rank
    qsort(otr_tup, tr.num_tr, sizeof(TR_TUP), compare_tr_rank);

	for (tr_ind = 0; tr_ind < MIN(num_wanted, tr.num_tr); tr_ind++) {
		/* Open the document vector and search for this given word
		 * in the con_wt array. The number stored in the LM vector
		 * is {tf.sum(df)}/{sum(tf).df}
		 */
	    dvec.id_num.id = otr_tup[tr_ind].did;
	    if (UNDEF == seek_vector (doc_fd, &dvec) ||
	        UNDEF == read_vector (doc_fd, &dvec)) {
    	    return UNDEF;
	    }

		docLen = (double)getDocumentLength(&dvec);
		// Similarity alone is not enough because we don't add in the uniform prior prob. P(D)
		// while calculating similarities.
		p_D_Q = exp(-otr_tup[tr_ind].sim/(float)collstats_numdocs);   

		// Iterate for every word in this document
		for (conwtp = dvec.con_wtp; conwtp < &dvec.con_wtp[dvec.num_conwt]; conwtp++) {
			if (collstats_freq[conwtp->con] == 0)
				continue;
			/* clarity += P(w|D) * P(D|Q)
			 * P(w|D) is to be computed as lambda *tf(w,D)/sum(tf, D) + (1-lambda)*df(w)/sum(df)
			 * tr->tr[tr_ind].sim contains the accumulated log P(D|Q) */
			p_w_D = lambda*conwtp->wt/docLen + (1-lambda)*collstats_freq[conwtp->con]/(double)totalDocFreq; 
			if (p_w_Q[conwtp->con] == 0) {
				// we are hitting on a new term
				termCount++;
				if (termCount > num_terms_wanted) {
					FREE(otr_tup);
					return;
				}
			}

			p_w_Q[conwtp->con] += p_w_D * p_D_Q; 
		}
	}

	FREE(otr_tup);
}

