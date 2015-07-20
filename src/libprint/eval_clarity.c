#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libevaluate/rrvec_trec_eval.c,v 11.0 1992/07/21 18:20:34 chrisb Exp chrisb $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 
 *
 * Reranks a tr file by reranking the documents by the KL divergence
 * between the document model and the estimated relevance model.
 * Ref: The paper by Croft on selective query expansion (Section 2.2)
 * No query expansion is done here. TBD later on.
 * To be used as a baseline for the TRLM experiments.
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
#include "collstat.h"
#include "dir_array.h"
#include "vector.h"
#include "retrieve.h"
#include "context.h"
#include "rr_vec.h"
#include "tr_vec.h"

static int tr_fd;
static char* tr_file;
static char* in_tr_file;
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
static float mixing_lambda ;
static long totalDocFreq;
static PROC_INST get_query;

static float *p_w_Q;				// conditional prob. of generating w from the current query Q
									// indexed by the word indices
static float *collstats_freq;		// frequency of the ith term in the collection
static long collstats_num_freq;		// total number of terms in collection
static long collstats_numdocs;		// Total number of documents in the collection

static float getTotalDocumentFreq();
static long getDocumentLength(VEC* vec);
static float compClarity(VEC* qvec);
static int comp_tr_tup_sim (TR_TUP* ptr1, TR_TUP* ptr2);
static int compare_con(void* this, void* that);
static int comp_did (TR_TUP* tr1, TR_TUP* tr2);

/* The collstat file is required for iterating over all the terms of the vocabulary
 * while computing the clarity score.
 * The query file is required to compute the product (summation of log) for all
 * the individual terms of Q. We require the tr file to walk through the top R
 * documents and get in the retrieval scores P(D|Q) from it.
 */
static SPEC_PARAM spec_args[] = {
    {"eval_clarity.get_query",        getspec_func, (char *) &get_query.ptab},
    {"eval_clarity.mixing_lambda", getspec_float, (char*)&mixing_lambda},
    {"eval_clarity.nnn_doc_file",        getspec_dbfile, (char *) &dvec_file},
    {"eval_clarity.doc_file.rmode",  getspec_filemode, (char *) &dvec_file_mode},
    {"eval_clarity.query_file",        getspec_dbfile, (char *) &qvec_file},
    {"eval_clarity.query_file.rmode",  getspec_filemode, (char *) &qvec_file_mode},
    {"eval_clarity.tr_file",        getspec_dbfile, (char *) &in_tr_file},
    {"eval_clarity.tr_file.rmode",     getspec_filemode, (char *) &tr_mode},
    {"eval_clarity.collstat_file",        getspec_dbfile, (char *) &collstat_file},
    {"eval_clarity.collstat_file.rmode",  getspec_filemode, (char *) &collstat_mode},
    TRACE_PARAM ("eval_clarity.trace")
};

static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

int
init_eval_clarity (spec, unused)
SPEC *spec;
char *unused;
{
	DIR_ARRAY dir_array;

    PRINT_TRACE (2, print_string, "Trace: entering/leaving init_eval_clarity");

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    if (UNDEF == (get_query.inst =
                  get_query.ptab->init_proc (spec, NULL)))
		return UNDEF;

    if (! VALID_FILE (dvec_file))
		return UNDEF;

    /* Open the query vector, LM weighted document vector and the tr files */
   	if (UNDEF == (q_fd = open_vector (qvec_file, qvec_file_mode)))
       	return (UNDEF);

   	if (UNDEF == (doc_fd = open_vector (dvec_file, dvec_file_mode)))
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
        dir_array.id_num = COLLSTAT_TOTWT; // COLLSTAT_COLLFREQ; // ; // Get the collection frequency list from the file
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
			p_w_Q = (float*) malloc (collstats_num_freq * sizeof(float));
			if (p_w_Q == NULL)
				return UNDEF;
        }

    }

	totalDocFreq = getTotalDocumentFreq();
    PRINT_TRACE (2, print_string, "Trace: leaving init_eval_clarity");
    return 1;
}

int
eval_clarity (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
	FILE*  output;
	int    status;
	QUERY_VECTOR query_vec;
	float clarity;

    PRINT_TRACE (2, print_string, "Trace: entering eval_clarity");
    output = VALID_FILE (out_file) ? fopen (out_file, "w") : stdout;
    if (NULL == output)
        return (UNDEF);

    if (in_file == NULL) in_file = in_tr_file;
	if (UNDEF == (tr_fd = open_tr_vec (in_file, tr_mode)))
		return (UNDEF);

	/* For each query */
    while (1 == (status = get_query.ptab->proc (NULL, &query_vec, get_query.inst))) {
	    if (query_vec.qid < global_start)
		    continue;
		if (query_vec.qid > global_end)
			break;

		clarity = compClarity ((VEC*)(query_vec.vector));
		fprintf(output, "%d %f\n", query_vec.qid, clarity);
	}

    if (UNDEF == close_tr_vec (tr_fd))
		return UNDEF;

    PRINT_TRACE (2, print_string, "Trace: leaving eval_clarity");
    return (1);
}


int
close_eval_clarity (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_eval_clarity");

	if ( UNDEF == close_vector(q_fd) )
		return UNDEF ;
	if ( UNDEF == close_vector(doc_fd) )
		return UNDEF;
    if (UNDEF == close_dir_array (collstats_fd))
	    return (UNDEF);

    if (UNDEF == get_query.ptab->close_proc (get_query.inst))
		return UNDEF;

	FREE(p_w_Q);

    PRINT_TRACE (2, print_string, "Trace: leaving close_eval_clarity");
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

static int compare_con(void* this, void* that) {
    CON_WT *this_conwt, *that_conwt;
    this_conwt = (CON_WT*) this;
    that_conwt = (CON_WT*) that;

    return this_conwt->con < that_conwt->con ? -1 : this_conwt->con == that_conwt->con ? 0 : 1;
}

/* This function computes P(w|Q) which is the approximation
 * of P(w|R).
 * Accumulate the probabilities P(w|Q) for every word w
 * encountered while reading in the top documents. */
static float compClarity(VEC* qvec)
{
	TR_VEC  tr;
	TR_TUP  *tr_tup;
	long    tr_ind;
	VEC     dvec;
	CON_WT  *conwtp, *this_conwt;
	float   p_w_D, p_D_Q, norm_tf = 0;
	float   docLen;
	float   kl_div;  // KL divergence of a document model from the relevance model
	long    con;
	CON_WT  *p_w_R;	// a compact array storing the term ids and the p_w_Q weight for that term
	int     vsize;	// vocabulary size of the union set
	int     i;
	float   p_w_Coll;

	// Initialize the P(w|R) values
	memset(p_w_Q, 0, sizeof(float)*collstats_num_freq);

    tr.qid = qvec->id_num.id;
    if (UNDEF == seek_tr_vec(tr_fd, &tr) ||
		UNDEF == read_tr_vec(tr_fd, &tr))
		return -1.0f;

	// In RLM we should:
	// 1. Iterate over the entire vocabulary. For efficiency the vocabulary here
	//    is the union of the set of terms present in the retrieved documents.
	// 2. Estimate P(w|R) for each term.
	//    This ensures that if any term 't' is not there in the top R docs, it should
	//    not have a zero P(t|R).
	// 3. Keep the top 1000 terms for the next step KL divergence calculation
	//
	// But iterating over the entire vocabulary is costly. So we do the calculations on the
	// union of terms present in the top ranked documents. The rest of the P(w|R) array
	// is filled up by collection probabilities (this is done in the initialization phase itself).

	// Estimate the relevance model...
	for (tr_ind = 0; tr_ind < tr.num_tr; tr_ind++) {
		tr_tup = &tr.tr[tr_ind];
		if (tr_tup->rel == 0)
			continue;
		
		/* Open the document vector and search for this given word
		 * in the con_wt array. The number stored in the LM vector
		 * is {tf.sum(df)}/{sum(tf).df}
		 */
	    dvec.id_num.id = tr.tr[tr_ind].did;
	    if (UNDEF == seek_vector (doc_fd, &dvec) ||
    	    UNDEF == read_vector (doc_fd, &dvec)) {
			return -1.0f;
	    }

		docLen = (float)getDocumentLength(&dvec);
		// LM similarity as stored in the LM vec is an approximation of log(P(Q|D))
		p_D_Q = tr_tup->sim;

		// Iterate for every word in this document
		for (conwtp = dvec.con_wtp; conwtp < &dvec.con_wtp[dvec.num_conwt]; conwtp++) {
			 // P(w|Q) = \sum D \in R {P(w|D) * P(D|Q)}
			 // P(w|D) is to be computed as lambda *tf(w,D)/sum(tf, D) + (1-lambda)*df(w)/sum(df)
			 // tr->tr[tr_ind].sim contains the accumulated log P(D|Q )
			p_w_D = mixing_lambda*conwtp->wt/docLen + (1-mixing_lambda)*collstats_freq[conwtp->con]/(float)totalDocFreq;
			p_w_Q[conwtp->con] += p_w_D * p_D_Q; 
		}
	}

	// Once we are done with the loop above, we know the values of P(w|R) for each w in V (the union of
	// the set of terms in the retrieved documents).
	// In the calcutaion of the KL divergence, this entire set of terms has to be taken into account.
	// Compact the array p_w_Q
	vsize = 0;
	for (con = 0; con < collstats_num_freq; con++) {
		if (p_w_Q[con] > 0 && collstats_freq[con] > 0) vsize++;
	}
	p_w_R = Malloc(vsize, CON_WT);

	for (i = 0, con = 0; con < collstats_num_freq; con++) {
		if (p_w_Q[con] > 0 && collstats_freq[con] > 0) {
			p_w_R[i].con = con;
			p_w_R[i].wt = p_w_Q[con];
			i++;
		}
	}

	// Print out the clarity score for this query.
	kl_div = 0;
	// We need to iterate over the entire set of terms in the retrieved result set.
	for (conwtp = p_w_R; conwtp < &p_w_R[vsize]; conwtp++) {
		p_w_Coll = collstats_freq[conwtp->con]/(float)totalDocFreq;	// P(w|coll) = cf(w)/collsize
		kl_div += conwtp->wt * log(conwtp->wt/p_w_Coll); // KL-Div
	}

	Free(p_w_R);
	return kl_div;
}

static int
comp_tr_tup_sim (ptr1, ptr2)
TR_TUP *ptr1;
TR_TUP *ptr2;
{
    if (ptr2->sim > ptr1->sim)
        return -1;
    if (ptr1->sim > ptr2->sim)
        return 1;
	return 0;
}

static int comp_did (TR_TUP* tr1, TR_TUP* tr2)
{
    if (tr1->did > tr2->did) return 1;
	    if (tr1->did < tr2->did) return -1;
		    return 0;
}

