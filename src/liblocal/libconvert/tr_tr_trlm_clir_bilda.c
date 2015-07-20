#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libevaluate/rrvec_trec_eval.c,v 11.0 1992/07/21 18:20:34 chrisb Exp chrisb $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 
 *
 * This file implements the CLIR inference chain for topical relevance models.
 * It takes as input the retrieved result of the documents in target language
 * and the corresponding LDA model obtained from retrieved documents of the source language.
 * It estimates the relevance model for the 
 * Here we implement the topical relevance model (two variants - uni/multi faceted).
 * Reranks a tr file by reranking the documents by the KL divergence
 * between the document model and the estimated relevance model.

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

// In Bi-LDA take the contribution from the corresponding topic on the source side.
// Assumption: # of topics is identical and distributed from the same theta.
// For this option, we need to run Bi-LDA instead of running LDAs separately
// on the source and target languages.
//
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
#include "docdesc.h"
#include "lda.h"
#include "wt.h"

static int tr_fd;
static int src_tr_fd;
static char* tr_file;
static char* src_tr_file;
static long tr_mode;
static int q_fd;
static int qrels_fd = UNDEF;
static char* qvec_file ;
static long qvec_file_mode;
static int doc_fd, src_doc_fd;
static char* dvec_file ;
static char* src_dvec_file ;
static long dvec_file_mode;
static int src_collstats_fd, tgt_collstats_fd;
static char* src_collstat_file, *tgt_collstat_file ;
static long collstat_mode ;
static float rerank_lambda;
static float src_mixing_lambda;
static float tgt_mixing_lambda;
static long src_totalDocFreq, tgt_totalDocFreq;
static PROC_INST get_query;

static float *p_w_Q;				// conditional prob. of generating w from the current query Q
									// indexed by the word indices
static float *src_collstats_freq, *tgt_collstats_freq;		// frequency of the ith term in the collection
static long src_collstats_num_freq, tgt_collstats_num_freq;		// total number of terms in collection
static int out_tr_file_fd;
static char *out_tr_file;
static long out_mode;
static char* qrels_file;
static long qrels_file_mode;

static float getTotalDocumentFreq(float* collstats_freq, long collstats_num_freq);
static long getDocumentLength(VEC* vec);
static int rerank_tr_trlm(VEC* qvec);
static int comp_tr_tup_sim (TR_TUP* ptr1, TR_TUP* ptr2);
static int comp_did (TR_TUP* tr1, TR_TUP* tr2);
static int compare_con(void* this, void* that);
static int fill_rel_info(TR_VEC* tr_vec);

extern float bilda_compute_marginalized_P_w_D_tgt(BiLDAModel* ip, CON_WT* conwtp, int docIndex);
extern float bilda_compute_marginalized_P_w_D_src(BiLDAModel* ip, CON_WT* conwtp, int docIndex);

typedef struct {
	SPEC* spec;
	BiLDAModel bilda;			// Joint LDA estimation for both the source and the target language documents
} STATIC_INFO;

static STATIC_INFO ip;
static char msg[256];

typedef struct {
	int topicId;
	float contrib;
} BestSrcTopic;

/* The collstat file is required for iterating over all the terms of the vocabulary
 * while computing the P(w|R) scores.
 * The query file is required to compute the product (summation of log) for all
 * the individual terms of Q. We require the tr file to walk through the top R
 * documents and get in the retrieval scores P(D|Q) from it.
 */
static SPEC_PARAM spec_args[] = {
    {"tr_tr_trlm_clir.get_query",        getspec_func, (char *) &get_query.ptab},
    {"tr_tr_trlm_clir.rerank_lambda", getspec_float, (char*)&rerank_lambda},
    {"tr_tr_trlm_clir.src_mixing_lambda", getspec_float, (char*)&src_mixing_lambda},
    {"tr_tr_trlm_clir.tgt_mixing_lambda", getspec_float, (char*)&tgt_mixing_lambda},
    {"tr_tr_trlm_clir.nnn_doc_file",        getspec_dbfile, (char *) &dvec_file},
    {"tr_tr_trlm_clir.src_nnn_doc_file",        getspec_dbfile, (char *) &src_dvec_file},
    {"tr_tr_trlm_clir.doc_file.rmode",  getspec_filemode, (char *) &dvec_file_mode},
    {"tr_tr_trlm_clir.query_file",        getspec_dbfile, (char *) &qvec_file},
    {"tr_tr_trlm_clir.query_file.rmode",  getspec_filemode, (char *) &qvec_file_mode},
    {"tr_tr_trlm_clir.tgt_tr_file",        getspec_dbfile, (char *) &tr_file},
    {"tr_tr_trlm_clir.src_tr_file",        getspec_dbfile, (char *) &src_tr_file},
    {"tr_tr_trlm_clir.tr_file.rmode",     getspec_filemode, (char *) &tr_mode},
    {"tr_tr_trlm_clir.out.tr_file", getspec_dbfile, (char *)&out_tr_file},
    {"tr_tr_trlm_clir.out.rwmode", getspec_filemode, (char *)&out_mode},
    {"tr_tr_trlm_clir.src_collstat_file",        getspec_dbfile, (char *) &src_collstat_file},
    {"tr_tr_trlm_clir.tgt_collstat_file",        getspec_dbfile, (char *) &tgt_collstat_file},
    {"tr_tr_trlm_clir.collstat_file.rmode",  getspec_filemode, (char *) &collstat_mode},
    {"tr_tr_trlm_clir.qrels_file",       getspec_dbfile, (char *) &qrels_file},
    {"tr_tr_trlm_clir.qrels_file.rmode", getspec_filemode, (char *) &qrels_file_mode},
    TRACE_PARAM ("tr_tr_trlm_clir.trace")
};

static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

int
init_tr_tr_trlm_clir_bilda (spec, unused)
SPEC *spec;
char *unused;
{
	DIR_ARRAY dir_array;

    PRINT_TRACE (2, print_string, "Trace: entering/leaving init_tr_tr_trlm_clir");
	ip.spec = spec;

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
   	if (UNDEF == (src_doc_fd = open_vector (src_dvec_file, dvec_file_mode)))
       	return (UNDEF);

    /* Open qrels file, if desired */
    if (VALID_FILE (qrels_file) &&
			UNDEF == (qrels_fd = open_rr_vec (qrels_file, qrels_file_mode)))
		return (UNDEF);

	if (!VALID_FILE (src_collstat_file))
		return UNDEF;
	if (!VALID_FILE (tgt_collstat_file))
		return UNDEF;

	if (UNDEF == (src_collstats_fd = open_dir_array (src_collstat_file, collstat_mode)) ||
		UNDEF == (tgt_collstats_fd = open_dir_array (tgt_collstat_file, collstat_mode)))
		return (UNDEF);

	// Read in collection frequencies
    dir_array.id_num = COLLSTAT_TOTWT; // Get the collection frequency list from the file
    if (1 != seek_dir_array (src_collstats_fd, &dir_array) ||
        1 != read_dir_array (src_collstats_fd, &dir_array)) {
        src_collstats_freq = NULL;
        src_collstats_num_freq = 0;
		return UNDEF;
    }
    else {
        // Read from file successful. Allocate 'freq' array and dump the
        // contents of the file in this list
		src_collstats_freq = (float*) dir_array.list;
        src_collstats_num_freq = dir_array.num_list / sizeof (float);
    }

	// Read in collection frequencies
    dir_array.id_num = COLLSTAT_TOTWT; // Get the collection frequency list from the file
    if (1 != seek_dir_array (tgt_collstats_fd, &dir_array) ||
        1 != read_dir_array (tgt_collstats_fd, &dir_array)) {
        tgt_collstats_freq = NULL;
        tgt_collstats_num_freq = 0;
		return UNDEF;
    }
    else {
        // Read from file successful. Allocate 'freq' array and dump the
        // contents of the file in this list
		tgt_collstats_freq = (float*) dir_array.list;
        tgt_collstats_num_freq = dir_array.num_list / sizeof (float);
		p_w_Q = (float*) malloc (tgt_collstats_num_freq * sizeof(float));
		if (p_w_Q == NULL)
			return UNDEF;
    }

	src_totalDocFreq = getTotalDocumentFreq(src_collstats_freq, src_collstats_num_freq);
	tgt_totalDocFreq = getTotalDocumentFreq(tgt_collstats_freq, tgt_collstats_num_freq);

	// Initialize the Bi-LDA model here...
	if (UNDEF == init_bilda_est(&ip.bilda, ip.spec))
		return UNDEF;

	// Load the bilingual dictionary in an in-memory hash table. This is required to
	// compute the alignment probabilities between source language and target language topics.
	if (UNDEF == init_text_bdict(ip.spec))
		return UNDEF;

    PRINT_TRACE (2, print_string, "Trace: leaving init_tr_tr_trlm_clir");
    return 1;
}

int
tr_tr_trlm_clir_bilda (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
	int    status;
	QUERY_VECTOR query_vec;

    PRINT_TRACE (2, print_string, "Trace: entering tr_tr_rlm");
    if (in_file == NULL) in_file = tr_file;
    if (out_file == NULL) out_file = out_tr_file;

	if (UNDEF == (tr_fd = open_tr_vec (in_file, tr_mode)) ||
		UNDEF == (src_tr_fd = open_tr_vec (src_tr_file, tr_mode)) ||
	    UNDEF == (out_tr_file_fd = open_tr_vec (out_file, SRDWR | SCREATE)))
		return (UNDEF);

	/* For each query */
    while (1 == (status = get_query.ptab->proc (NULL, &query_vec, get_query.inst))) {
	    if (query_vec.qid < global_start)
		    continue;
		if (query_vec.qid > global_end)
			break;

		if (UNDEF == rerank_tr_trlm ((VEC*)(query_vec.vector)))
			return UNDEF;
	}

    if (UNDEF == close_tr_vec (tr_fd))
		return UNDEF;
    if (UNDEF == close_tr_vec (out_tr_file_fd))
		return UNDEF;

    PRINT_TRACE (2, print_string, "Trace: leaving tr_tr_rlm");
    return (1);
}


int
close_tr_tr_trlm_clir_bilda (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_tr_tr_trlm_clir");

	if ( UNDEF == close_vector(q_fd) )
		return UNDEF ;
	if ( UNDEF == close_vector(doc_fd) )
		return UNDEF;
	if ( UNDEF == close_vector(src_doc_fd) )
		return UNDEF;
    if (UNDEF == close_dir_array (src_collstats_fd) ||
		UNDEF == close_dir_array (tgt_collstats_fd))
	    return (UNDEF);
    if (UNDEF == get_query.ptab->close_proc (get_query.inst))
		return UNDEF;
    if (UNDEF != qrels_fd && UNDEF == close_rr_vec (qrels_fd))
		        return (UNDEF);

	FREE(p_w_Q);

	close_bilda_est(&ip.bilda);

	// Free up the bilingual dictionary
	close_text_bdict();

    PRINT_TRACE (2, print_string, "Trace: leaving close_tr_tr_trlm_clir");
    return (0);
}

static float getTotalDocumentFreq(float* collstats_freq, long collstats_num_freq) {
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

static int getProbWordFromSrcTranslations(BiLDAModel* bilda, float* tgt_p_w_Q) {
	TR_TUP *tr_tup;
	long    tr_ind;
	VEC     dvec;
	CON_WT* conwtp;
	WordTranslation *t;
	long    *tgt_con;
	int     num_target_translations;
	float   p_D_Q, mixwt;
	int     rel_count = 0;
	TR_VEC  *src_tr = bilda->ldaModel.tr_vec;		// tr vector of the source language

	// Iterate for every word in the source vocabulary and fill up
	// the corresponding contributions of the target language	
	for (tr_ind = 0; tr_ind < src_tr->num_tr; tr_ind++) {
		tr_tup = src_tr->tr + tr_ind;
		if (tr_tup->rel == 0)
			continue;
		// Get the document vector to iterate over the words
	    dvec.id_num.id = tr_tup->did;
	    if (UNDEF == seek_vector (src_doc_fd, &dvec) ||
			UNDEF == read_vector (src_doc_fd, &dvec)) {
   	    	return UNDEF;
	    }

		p_D_Q = tr_tup->sim;	// P(Q|D) is the standard LM similarity stored in the retrieval score

		// Iterate for every word in this document
		for (conwtp = dvec.con_wtp; conwtp < &dvec.con_wtp[dvec.ctype_len[0]]; conwtp++) {
			if (src_collstats_freq[conwtp->con] == 0)
				continue;
			// Query the bi-lingual dictionary to get the translations for
			// this particular word in the target language.
			if (UNDEF == getTranslation(conwtp->con, &t)) {
				// this word does not belong to the dictionary. skip it...
				continue;
			}
			// Iterate over the target language cons and add the corresponding contributions.
			num_target_translations = 0;
			for (tgt_con = &t->cons[1]; tgt_con < &t->cons[t->numTranslations]; tgt_con++) {
				if (*tgt_con >= 0 && *tgt_con < tgt_collstats_num_freq)
					num_target_translations++;
			}
			for (tgt_con = &t->cons[1]; tgt_con < &t->cons[t->numTranslations]; tgt_con++) {
				if (*tgt_con >= 0 && *tgt_con < tgt_collstats_num_freq) {
					mixwt = (src_mixing_lambda * bilda_compute_marginalized_P_w_D_src(bilda, conwtp, rel_count) +
							(1-src_mixing_lambda)*src_collstats_freq[conwtp->con]/(float)src_totalDocFreq)*p_D_Q/(float) num_target_translations; 
					tgt_p_w_Q[*tgt_con] += mixwt;
				}
			}
		}
		rel_count++;
	}
	return 0;
}

/* This function computes P(w|Q) which is the approximation of P(w|R).
 * Do an LDA estimation over the top R pseudo relevant documents, and use
 * the probabilities P(w|z).P(z|d) marginalized over all topics. 
 * Accumulate the probabilities P(w|Q) for every word w
 * encountered while reading in the top documents. */
static int rerank_tr_trlm(VEC* qvec)
{
	TR_VEC  tr, src_tr;
	TR_TUP  *tr_tup;
	long    tr_ind, con;
	VEC     dvec;
	CON_WT  *conwtp, *this_conwt;
	float   p_w_D, p_D_Q, norm_tf = 0;
	float   docLen;
	TR_TUP* otr_tup; // ordered by rank
	float   kl_div;  // KL divergence of a document model from the relevance model
	TR_VEC  out_tr_vec;
	float   this_kl_div;
	CON_WT  *p_w_R;	// a compact array storing the term ids and the p_w_Q weight for that term
	int     vsize;	// vocabulary size of the union set
	int     i, k, rel_count = 0;
	float   p_Z_Q_src, p_Z_Q_tgt;	// probability of a target language word
   									// being generated from a target language topic
									// and from a source language topic (aligned) respectively.
	float   tgt_lda_wt;

	// Set the conditional probs. P(w|Q)s to zero
	memset(p_w_Q, 0, tgt_collstats_num_freq * sizeof(float));

    tr.qid = qvec->id_num.id;
    if (UNDEF == seek_tr_vec(tr_fd, &tr) ||
		UNDEF == read_tr_vec(tr_fd, &tr))
		return(UNDEF);

    src_tr.qid = qvec->id_num.id;
    if (UNDEF == seek_tr_vec(src_tr_fd, &src_tr) ||
		UNDEF == read_tr_vec(src_tr_fd, &src_tr))
		return(UNDEF);

	otr_tup = (TR_TUP*) malloc (sizeof(TR_TUP) * tr.num_tr);
	if (otr_tup == NULL)
		return UNDEF;

	/* Call the LDA estimator to estimate P(w|z) and P(z|d)s.
	 * This is LDA estimation for the target language. */
	if (UNDEF == init_bilda_est_inst(&ip.bilda, src_doc_fd, doc_fd, &src_tr, &tr, qvec, 0)) {
		/* if there is some error, then simply copy the existing tr_vec
		 * into output... */
		if (UNDEF == seek_tr_vec(out_tr_file_fd, &tr) ||
	    	UNDEF == write_tr_vec(out_tr_file_fd, &tr)) {
	    	return (UNDEF);
		}
		fprintf(stderr, "Couldn't rerank tr file for query %d\n", tr.qid);
		FREE(otr_tup);
		return 1;
	}
	bilda_est(&ip.bilda);

	// Fill up the source contributions into the p_w_Q array
	if (UNDEF == getProbWordFromSrcTranslations(&(ip.bilda), p_w_Q))
		return UNDEF;

	// P(z^B|D^B): probabilities from the target side
	for (tr_ind = 0; tr_ind < tr.num_tr; tr_ind++) {
		tr_tup = &tr.tr[tr_ind];
		if (tr_tup->rel == 0)
			continue;
		// Get the document vector to iterate over the words
	    dvec.id_num.id = tr_tup->did;
	    if (UNDEF == seek_vector (doc_fd, &dvec) ||
			UNDEF == read_vector (doc_fd, &dvec)) {
   	    	return UNDEF;
	    }

		p_D_Q = tr_tup->sim;	// P(Q|D) is the standard LM similarity stored in the retrieval score

		// Iterate for every word in this document
		for (conwtp = dvec.con_wtp; conwtp < &dvec.con_wtp[dvec.ctype_len[0]]; conwtp++) {
			// For each topic in the target language
			tgt_lda_wt = bilda_compute_marginalized_P_w_D_tgt(&ip.bilda, conwtp, rel_count);
			p_w_D = tgt_mixing_lambda * tgt_lda_wt + (1-tgt_mixing_lambda) * tgt_collstats_freq[conwtp->con]/(float)tgt_totalDocFreq;
			p_w_Q[conwtp->con] += p_w_D * p_D_Q;
		}	
		rel_count++;
	}

	// Once we are done with the loop above, we know the values of P(w|R) for each w in V (the union of
	// the set of terms in the retrieved documents).
	// In the calcutaion of the KL divergence, this entire set of terms has to be taken into account.
	// Compact the array p_w_Q
	vsize = 0;
	for (con = 0; con < tgt_collstats_num_freq; con++) {
		if (p_w_Q[con] > 0 && tgt_collstats_freq[con] > 0) vsize++;
	}
	p_w_R = Malloc(vsize, CON_WT);

	for (i = 0, con = 0; con < tgt_collstats_num_freq; con++) {
		if (p_w_Q[con] > 0 && tgt_collstats_freq[con] > 0) {
			p_w_R[i].con = con;
			p_w_R[i].wt = p_w_Q[con];
			i++;
		}
	}
	
	// In this part we calculate the KL-divergence between a document model
	// and the relevance model and rerank the documents based on this score. 
	// This loop runs over all the retrieved documents
	// since we need to rerank all (not only the top pseudo relevant) retrieved documents.
	// In the loop compute KL-Div(D,R)...
	for (tr_ind = 0; tr_ind < tr.num_tr; tr_ind++) {
		tr_tup = &tr.tr[tr_ind];
		kl_div = 0;

	    dvec.id_num.id = tr_tup->did;
	    if (UNDEF == seek_vector (doc_fd, &dvec) ||
	        UNDEF == read_vector (doc_fd, &dvec)) {
    	    return UNDEF;
	    }
		docLen = (float)getDocumentLength(&dvec);
		// Instead of iterating over the entire vocab, we
		// iterate over the terms in this document
		for (conwtp = p_w_R; conwtp < &p_w_R[vsize]; conwtp++) {
			// This search gives the tf (t_i|d) i.e.
			// the term frequency of the current term in the current document.
			// It might be zero.
	        this_conwt = (CON_WT*) bsearch(conwtp, dvec.con_wtp, dvec.ctype_len[0], sizeof(CON_WT), compare_con);
			norm_tf = (this_conwt == NULL)? 0 : this_conwt->wt/docLen;

			p_w_D = rerank_lambda*norm_tf + (1-rerank_lambda)*tgt_collstats_freq[conwtp->con]/(float)tgt_totalDocFreq;
			this_kl_div = conwtp->wt * log(conwtp->wt/p_w_D); // KL-Div
			kl_div += this_kl_div;
		}
		otr_tup[tr_ind].sim = kl_div;   // write back the kl_div in the sim field 
										// so as to sort it later on this field
		otr_tup[tr_ind].did = tr_tup->did;
		otr_tup[tr_ind].iter = tr_tup->iter;
		otr_tup[tr_ind].action = tr_tup->action;
	}

	// sort the otr_tup before freeing it.
    qsort(otr_tup, tr.num_tr, sizeof(TR_TUP), comp_tr_tup_sim);
	// write back the rank values
	for (tr_ind = 0; tr_ind < tr.num_tr; tr_ind++) {
		otr_tup[tr_ind].rank = tr_ind + 1; 
	}

	// resort on did (SMART standard)
    qsort(otr_tup, tr.num_tr, sizeof(TR_TUP), comp_did);

	out_tr_vec.qid = tr.qid;
	out_tr_vec.num_tr = tr.num_tr;
	out_tr_vec.tr = otr_tup;

	// write back the relevance values (if there are any)
	if (UNDEF == fill_rel_info(&out_tr_vec))
		return UNDEF;

	/* output tr_vec to tr_file */
	if (UNDEF == seek_tr_vec(out_tr_file_fd, &out_tr_vec) ||
	    UNDEF == write_tr_vec(out_tr_file_fd, &out_tr_vec)) {
	    return (UNDEF);
	}

	// Cleanup...
	FREE(otr_tup);
	Free(p_w_R);

	close_bilda_est_inst(&ip.bilda);

	return 0;
}

static int fill_rel_info(TR_VEC* tr_vec) {
	RR_VEC   qrels;
    register RR_TUP *qr_ptr, *end_qr_ptr;
    long i;
    int status;

	if (qrels_fd != UNDEF) {
		qrels.qid = tr_vec->qid;
		if (1 == (status = seek_rr_vec (qrels_fd, &qrels)) &&
	    	1 == (status = read_rr_vec (qrels_fd, &qrels))) {
		    qr_ptr = qrels.rr;
		    end_qr_ptr = &qrels.rr[qrels.num_rr];
	    	for (i = 0; i < tr_vec->num_tr; i++) {
				tr_vec->tr[i].rel = 0;
				while (qr_ptr < end_qr_ptr && tr_vec->tr[i].did >qr_ptr->did)
				    qr_ptr++;
				if (qr_ptr < end_qr_ptr && tr_vec->tr[i].did == qr_ptr->did) {
				    qr_ptr++;
				    tr_vec->tr[i].rel = 1;
				}
		    }
		}
		else if (status == UNDEF)
	    	return (UNDEF);
	}
	return 0;
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

