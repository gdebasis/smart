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
static long tr_mode, rwmode;
static int q_fd;
static char* qvec_file, *expqvec_file;
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
static CON_WT* conwt_buf, *tmp_buf;
static int conwt_buf_size = 2048;
static long* ctype_len_buff;
static int ctype_len_buff_size = 2;
static float fbOrigWeight; // similar to Indri shit!!

static float *p_w_Q;				// conditional prob. of generating w from the current query Q
									// indexed by the word indices
static float *collstats_freq;		// frequency of the ith term in the collection
static long collstats_num_freq;		// total number of terms in collection
static long collstats_numdocs;		// Total number of documents in the collection
static long nexpansionTerms;

static float getTotalDocumentFreq();
static long getDocumentLength(VEC* vec);
static int expand_rlm(VEC* qvec, int);
static int compare_con(void* this, void* that);
static int compare_conwt(void* this, void* that);

/* The collstat file is required for iterating over all the terms of the vocabulary
 * while computing the clarity score.
 * The query file is required to compute the product (summation of log) for all
 * the individual terms of Q. We require the tr file to walk through the top R
 * documents and get in the retrieval scores P(D|Q) from it.
 */
static SPEC_PARAM spec_args[] = {
    {"tr_tr_rlm_qexp.get_query",        getspec_func, (char *) &get_query.ptab},
    {"tr_tr_rlm_qexp.mixing_lambda", getspec_float, (char*)&mixing_lambda},
    {"tr_tr_rlm_qexp.nnn_doc_file",        getspec_dbfile, (char *) &dvec_file},
    {"tr_tr_rlm_qexp.doc_file.rmode",  getspec_filemode, (char *) &dvec_file_mode},
    {"tr_tr_rlm_qexp.query_file",        getspec_dbfile, (char *) &qvec_file},
    {"tr_tr_rlm_qexp.query_file.rmode",  getspec_filemode, (char *) &qvec_file_mode},
    {"tr_tr_rlm_qexp.out_query_file",        getspec_dbfile, (char *) &expqvec_file},
    {"tr_tr_rlm_qexp.out_file.rwmode",  getspec_filemode, (char *) &rwmode},
    {"tr_tr_rlm_qexp.in_tr_file",        getspec_dbfile, (char *) &in_tr_file},
    {"tr_tr_rlm_qexp.tr_file.rmode",     getspec_filemode, (char *) &tr_mode},
    {"tr_tr_rlm_qexp.collstat_file",        getspec_dbfile, (char *) &collstat_file},
    {"tr_tr_rlm_qexp.collstat_file.rmode",  getspec_filemode, (char *) &collstat_mode},
    {"tr_tr_rlm_qexp.nterms",        getspec_long, (char *) &nexpansionTerms},
    {"tr_tr_rlm_qexp.fbweight",        getspec_float, (char *) &fbOrigWeight},
    TRACE_PARAM ("tr_tr_rlm_qexp.trace")
};

static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

int
init_tr_tr_rlm_qexp (spec, unused)
SPEC *spec;
char *unused;
{
	DIR_ARRAY dir_array;

    PRINT_TRACE (2, print_string, "Trace: entering/leaving init_tr_tr_rlm_qexp");

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    if (UNDEF == (get_query.inst =
                  get_query.ptab->init_proc (spec, NULL)))
		return UNDEF;

    if (! VALID_FILE (dvec_file))
		return UNDEF;

	conwt_buf = Malloc(conwt_buf_size, CON_WT*); 
	tmp_buf = Malloc(conwt_buf_size, CON_WT*);
	ctype_len_buff = Malloc(ctype_len_buff_size, long);
	if (!conwt_buf || !tmp_buf || !ctype_len_buff)
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
    PRINT_TRACE (2, print_string, "Trace: leaving init_tr_tr_rlm_qexp");
    return 1;
}

int
tr_tr_rlm_qexp (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
	int    status;
	QUERY_VECTOR query_vec;
	int    exp_q_fd;

    PRINT_TRACE (2, print_string, "Trace: entering tr_tr_rlm_qexp");
    if (in_file == NULL) in_file = in_tr_file;
    if (out_file == NULL) out_file = expqvec_file;

	if (UNDEF == (tr_fd = open_tr_vec (in_file, tr_mode)))
		return (UNDEF);
    if (UNDEF == (exp_q_fd = open_vector (out_file, rwmode|SCREATE)))
		return (UNDEF);

	/* For each query */
    while (1 == (status = get_query.ptab->proc (NULL, &query_vec, get_query.inst))) {
	    if (query_vec.qid < global_start)
		    continue;
		if (query_vec.qid > global_end)
			break;

		// Initialize the P(w|R) values by Dirichlet smoothing
		memset(p_w_Q, 0, sizeof(float)*collstats_num_freq);

		if (UNDEF == expand_rlm ((VEC*)(query_vec.vector), exp_q_fd))
			return UNDEF;
	}

    if (UNDEF == close_tr_vec (tr_fd))
		return UNDEF;
	if ( UNDEF == close_vector(exp_q_fd) )
		return UNDEF ;

    PRINT_TRACE (2, print_string, "Trace: leaving tr_tr_rlm_qexp");
    return (1);
}


int
close_tr_tr_rlm_qexp (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_tr_tr_rlm_qexp");

	if ( UNDEF == close_vector(q_fd) )
		return UNDEF ;
	if ( UNDEF == close_vector(doc_fd) )
		return UNDEF;
    if (UNDEF == close_dir_array (collstats_fd))
	    return (UNDEF);

    if (UNDEF == get_query.ptab->close_proc (get_query.inst))
		return UNDEF;

	FREE(p_w_Q);
	FREE(conwt_buf);
	FREE(tmp_buf);

    PRINT_TRACE (2, print_string, "Trace: leaving close_tr_tr_rlm_qexp");
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

	// Read weights and add them up.
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
static int expand_rlm(VEC* qvec, int exp_q_fd)
{
	TR_VEC  tr;
	TR_TUP  *tr_tup;
	long    tr_ind;
	VEC     dvec, exp_qvec;
	CON_WT  *conwtp, *this_conwt, *new_conwtp;
	float   p_w_D, p_D_Q, norm_tf = 0;
	float   docLen;
	long    con;
	CON_WT  *p_w_R;	// a compact array storing the term ids and the p_w_Q weight for that term
	int     vsize;	// vocabulary size of the union set
	int     i, numHigherCtypes, totalWords;

    tr.qid = qvec->id_num.id;
    if (UNDEF == seek_tr_vec(tr_fd, &tr) ||
		UNDEF == read_tr_vec(tr_fd, &tr))
		return(UNDEF);

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
   	    	return UNDEF;
	    }

		docLen = (float)getDocumentLength(&dvec);
		// LM similarity as stored in the LM vec is an approximation of log(P(Q|D))
		p_D_Q = tr_tup->sim; //exp(tr_tup->sim)/(float)collstats_numdocs;

		// Iterate for every word in this document
		for (conwtp = dvec.con_wtp; conwtp < &dvec.con_wtp[dvec.ctype_len[0]]; conwtp++) {
			 // P(w|Q) = \sum D \in R {P(w|D) * P(D|Q)}
			 // P(w|D) is to be computed as lambda *tf(w,D)/sum(tf, D) + (1-lambda)*df(w)/sum(df)
			 // tr->tr[tr_ind].sim contains the accumulated log P(D|Q )
			p_w_D = mixing_lambda*conwtp->wt/docLen + (1-mixing_lambda)*collstats_freq[conwtp->con]/(float)totalDocFreq;
			p_w_Q[conwtp->con] += p_w_D * p_D_Q; 
		}
	}

	// Once we are done with the loop above, we know the values of P(w|R) for each w in V (the union of
	// the set of terms in the retrieved documents).
	// Compact the array p_w_Q into p_w_R and then sort it.
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

	qsort(p_w_R, vsize, sizeof(CON_WT), compare_conwt);
	if (qvec->ctype_len[0] + nexpansionTerms > conwt_buf_size) {
		conwt_buf_size = twice(qvec->ctype_len[0] + nexpansionTerms);
		conwt_buf = Realloc(conwt_buf, conwt_buf_size, CON_WT*);
		tmp_buf = Realloc(tmp_buf, conwt_buf_size, CON_WT*);
		if (!conwt_buf || !tmp_buf)
			return UNDEF;
	}

	// copy the original query words in buffer
	memcpy(conwt_buf, qvec->con_wtp, qvec->num_conwt*sizeof(CON_WT));
	for (this_conwt = conwt_buf; this_conwt < &conwt_buf[qvec->num_conwt]; this_conwt++)
		this_conwt->wt = fbOrigWeight * this_conwt->wt;

	// make sure we copy new words at the end of the expanded vector 
	for (i = 0, this_conwt = p_w_R; this_conwt < &p_w_R[vsize] && i < nexpansionTerms; this_conwt++) {
		if (!bsearch(this_conwt, conwt_buf, qvec->ctype_len[0], sizeof(CON_WT), compare_con)) {
			//memcpy(&conwt_buf[qvec->num_conwt + i], this_conwt, sizeof(CON_WT));
			conwt_buf[qvec->num_conwt + i].con = this_conwt->con;
			conwt_buf[qvec->num_conwt + i].wt = 1-fbOrigWeight;
			i++;
		}
	}
	nexpansionTerms = i;

	// re-arrange the vector to bring the words (ctype 0s) infront of the higher ctypes.
	numHigherCtypes = qvec->num_conwt - qvec->ctype_len[0];	
	totalWords = qvec->ctype_len[0] + nexpansionTerms;
	memcpy(tmp_buf, &conwt_buf[qvec->ctype_len[0]], numHigherCtypes*sizeof(CON_WT)); // copy higher ctypes in tmp buff
	memcpy(&conwt_buf[qvec->ctype_len[0]], &conwt_buf[qvec->num_conwt], nexpansionTerms*sizeof(CON_WT)); // copy new words at the end of existing words
	memcpy(&conwt_buf[totalWords], tmp_buf, numHigherCtypes*sizeof(CON_WT)); // copy phrases at the end of new words

	// re-sort the words and phrases by concept numbers
	qsort(conwt_buf, totalWords, sizeof(CON_WT), compare_con);
	qsort(&conwt_buf[totalWords], numHigherCtypes, sizeof(CON_WT), compare_con);

	EXT_NONE(exp_qvec.id_num.ext);
	exp_qvec.id_num.id = qvec->id_num.id;
	exp_qvec.num_conwt = qvec->num_conwt + nexpansionTerms;
	exp_qvec.con_wtp = conwt_buf;

	if (qvec->num_ctype > ctype_len_buff_size) {
		ctype_len_buff_size = qvec->num_ctype;
		ctype_len_buff = Realloc(ctype_len_buff, ctype_len_buff_size, long);
		if (!ctype_len_buff)
			return UNDEF;
	}
	memcpy(ctype_len_buff, qvec->ctype_len, qvec->num_ctype*sizeof(long));
	exp_qvec.num_ctype = qvec->num_ctype;
	exp_qvec.ctype_len = ctype_len_buff;
	exp_qvec.ctype_len[0] += nexpansionTerms;

	if (UNDEF == seek_vector (exp_q_fd, &exp_qvec))
		return UNDEF;
	if (UNDEF == write_vector (exp_q_fd, &exp_qvec))
		return (UNDEF);

	Free(p_w_R);
	return 0;
}

static void swap(CON_WT* this, CON_WT* that) {
	CON_WT tmp;
	tmp = *this;
	*this = *that;
	*that = tmp;
}

static int compare_conwt(void* this, void* that) {
	CON_WT *this_conwt, *that_conwt;
	this_conwt = (CON_WT*) this;
	that_conwt = (CON_WT*) that;

	return this_conwt->wt < that_conwt->wt ? 1 : -1;
}

