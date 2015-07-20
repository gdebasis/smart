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
static char* qvec_file ;
static long qvec_file_mode;
static int doc_fd, src_doc_fd;
static char* dvec_file ;
static char* src_dvec_file ;
static long dvec_file_mode;
static long src_totalDocFreq, tgt_totalDocFreq;
static PROC_INST get_query;

typedef struct {
	long src_con;
	long tgt_con;
} ConPair; 

typedef struct {
	ConPair conpair;
	char* src_word;
	char* tgt_word;
	float corr;
	UT_hash_handle hh;
}
WordCorrelation;

static int doSrcTgtLDA(VEC* qvec);
static int comp_wordpair_corr (WordCorrelation* ptr1, WordCorrelation* ptr2);

typedef struct {
	SPEC* spec;
	LDAModel src_ldamodel;		// LDA model for the source language
	LDAModel target_ldamodel;	// LDA model for the target language
	char* srcmodel_name;
	WordCorrelation *corrTable;
	int numCorrelations;
} STATIC_INFO;

static STATIC_INFO ip;
static char msg[256];

static SPEC_PARAM spec_args[] = {
    {"tr_tr_trlm_clir.get_query",        getspec_func, (char *) &get_query.ptab},
    {"tr_tr_trlm_clir.nnn_doc_file",        getspec_dbfile, (char *) &dvec_file},
    {"tr_tr_trlm_clir.src_nnn_doc_file",        getspec_dbfile, (char *) &src_dvec_file},
    {"tr_tr_trlm_clir.doc_file.rmode",  getspec_filemode, (char *) &dvec_file_mode},
    {"tr_tr_trlm_clir.query_file",        getspec_dbfile, (char *) &qvec_file},
    {"tr_tr_trlm_clir.query_file.rmode",  getspec_filemode, (char *) &qvec_file_mode},
    {"tr_tr_trlm_clir.tgt_tr_file",        getspec_dbfile, (char *) &tr_file},
    {"tr_tr_trlm_clir.src_tr_file",        getspec_dbfile, (char *) &src_tr_file},
    {"tr_tr_trlm_clir.tr_file.rmode",     getspec_filemode, (char *) &tr_mode},
    {"tr_tr_trlm_clir.src_ldamodel_name", getspec_string, (char *) &ip.srcmodel_name},
    {"tr_tr_trlm_clir.num_correlations", getspec_int, (char *) &ip.numCorrelations},
    TRACE_PARAM ("tr_tr_trlm_clir.trace")
};

static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static int insertWordCorrelation(long src_con, long tgt_con, char* src_word, char* tgt_word, float wordpair_corr);

int
init_tr_tr_trlm_clir_dict (spec, unused)
SPEC *spec;
char *unused;
{
    PRINT_TRACE (2, print_string, "Trace: entering/leaving init_tr_tr_trlm_clir_dict");
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

	if (UNDEF == init_lda_est(&ip.src_ldamodel, ip.spec) ||
		UNDEF == init_lda_est(&ip.target_ldamodel, ip.spec))
		return UNDEF;

	// Load the bilingual dictionary in an in-memory hash table. This is required to
	// compute the alignment probabilities between source language and target language topics.
	if (UNDEF == init_text_bdict(ip.spec))
		return UNDEF;

	ip.corrTable = NULL;
    PRINT_TRACE (2, print_string, "Trace: leaving init_tr_tr_trlm_clir_dict");
    return 1;
}

int
tr_tr_trlm_clir_dict (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
	int    status;
	FILE*  out_fd;
	QUERY_VECTOR query_vec;
	WordCorrelation *pcorr, *tmp;

    PRINT_TRACE (2, print_string, "Trace: entering tr_tr_rlm");
    if (in_file == NULL) in_file = tr_file;
    if (out_file == NULL) {
		set_error(3, "Output file missing", "tr_tr_trlm_clir_dict"); 
		return UNDEF;
	}
	out_fd = fopen(out_file, "w");
	if (!out_fd)
		return UNDEF;

	if (UNDEF == (tr_fd = open_tr_vec (in_file, tr_mode)) ||
		UNDEF == (src_tr_fd = open_tr_vec (src_tr_file, tr_mode)))
		return (UNDEF);

	/* For each query */
    while (1 == (status = get_query.ptab->proc (NULL, &query_vec, get_query.inst))) {
	    if (query_vec.qid < global_start)
		    continue;
		if (query_vec.qid > global_end)
			break;

		if (UNDEF == doSrcTgtLDA ((VEC*)(query_vec.vector)))
			return UNDEF;
	}

	// Display the top correlating words...
	HASH_SORT(ip.corrTable, comp_wordpair_corr);
	HASH_ITER(hh, ip.corrTable, pcorr, tmp) {
		fprintf(out_fd, "%s\t%s\t%f\n", pcorr->src_word, pcorr->tgt_word, pcorr->corr);
	}	   

    if (UNDEF == close_tr_vec (tr_fd))
		return UNDEF;

	fclose(out_fd);
    PRINT_TRACE (2, print_string, "Trace: leaving tr_tr_rlm");
    return (1);
}


int
close_tr_tr_trlm_clir_dict (inst)
int inst;
{
	WordCorrelation *pcorr, *tmp;
    PRINT_TRACE (2, print_string, "Trace: entering close_tr_tr_trlm_clir_dict");

	if ( UNDEF == close_vector(q_fd) )
		return UNDEF ;
	if ( UNDEF == close_vector(doc_fd) )
		return UNDEF;
	if ( UNDEF == close_vector(src_doc_fd) )
		return UNDEF;
    if (UNDEF == get_query.ptab->close_proc (get_query.inst))
		return UNDEF;

	close_lda_est(&ip.src_ldamodel);
	close_lda_est(&ip.target_ldamodel);

	HASH_ITER(hh, ip.corrTable, pcorr, tmp) {
		/* ... it is safe to delete and free s here */
		HASH_DEL(ip.corrTable, pcorr);
		free(pcorr);
	}

	// Free up the bilingual dictionary
	close_text_bdict();

    PRINT_TRACE (2, print_string, "Trace: leaving close_tr_tr_trlm_clir_dict");
    return (0);
}

// Maintain a sorted array (descending ordered) of top word pair correlations.
// If the given element is less than the last element then ignore it, else
// insert it in the last pos and resort the array.  
static int insertWordCorrelation(long src_con, long tgt_con, char* src_word, char* tgt_word, float wordpair_corr) {
	WordCorrelation *p;
	WordCorrelation key;

	memset(&key, 0, sizeof(WordCorrelation));
	key.conpair.src_con = src_con;
	key.conpair.tgt_con = tgt_con;

	HASH_FIND(hh, ip.corrTable, &key.conpair, sizeof(ConPair), p);
	if (p == NULL) {
		// New word pair
		p = Malloc (1, WordCorrelation);
		if (!p)
			return UNDEF;
		p->conpair.src_con = src_con;
		p->conpair.tgt_con = tgt_con;
		p->src_word = src_word;
		p->tgt_word = tgt_word;
		p->corr = wordpair_corr;
		HASH_ADD(hh, ip.corrTable, conpair, sizeof(ConPair), p);
	}
	else {
		// update this entry
		p->corr += wordpair_corr;
	}
	return 1;
}

static int computeWordAlignmentProbs(LDAModel* src, LDAModel* tgt, float **jd) {
	int i, j, k, k_prime;
	float corr;
	float ws_zs, zs_zt, zt_wt, sum_tgt, wordpair_corr;
	long src_con, tgt_con;
	char *src_word, *tgt_word;
	ConMap *e;
	TopicWord *src_twords, *tgt_twords, *sw, *tw;

	src_twords = Malloc (src->twords * src->K, TopicWord);
	if (!src_twords)
		return UNDEF;
	tgt_twords = Malloc (tgt->twords * tgt->K, TopicWord);
	if (!tgt_twords)
		return UNDEF;

	// Extract out the top <num words> terms from each source and target topics.
	// This avoids computing correlation over the entire vocabulary.
	
	for (k = 0; k < src->K; k++) {
		getMostLikelyWords(src, k, src_twords + k * src->twords, src->twords);
	}
	for (k_prime = 0; k_prime < tgt->K; k_prime++) {
		getMostLikelyWords(tgt, k_prime, tgt_twords + k_prime * tgt->twords, tgt->twords);
	}

	for (sw = src_twords; sw < src_twords + src->twords * src->K; sw++) {
		src_con = wid_to_con(src, sw->w);
		if (UNDEF == src_con)
	    	continue;
		if (UNDEF == wid_to_word(src, sw->w, &e))
			continue;
		src_word = e->word;

		for (tw = tgt_twords; tw < tgt_twords + tgt->twords * tgt->K; tw++) {
	    	tgt_con = wid_to_con(tgt, tw->w);
		    if (UNDEF == tgt_con)
				continue;
			if (UNDEF == wid_to_word(tgt, tw->w, &e))
				continue;
			tgt_word = e->word;

		    wordpair_corr = 0;
		    for (k = 0; k < src->K; k++) {
				ws_zs = p_w_z(src, k, sw->w);
				sum_tgt = 0;
				for (k_prime = 0; k_prime < tgt->K; k_prime++) {
				    zs_zt = jd[k][k_prime];
				    zt_wt = p_w_z(tgt, k_prime, tw->w);
				    sum_tgt += zs_zt * zt_wt;
				}
				wordpair_corr += ws_zs * sum_tgt;
			}
		    wordpair_corr /= (float)src->K;
		    if (UNDEF == insertWordCorrelation(src_con, tgt_con, src_word, tgt_word, wordpair_corr))
				return UNDEF;
		}
	}
	return 1;
}

// Return a matrix the (i,j)th entry of which contains the joint
// probability distribution of P(z^B_k|z^E_k') where E is the
// source language and B is the target language.
//
// The alignment is done as follows.
// For every word in the source language, get the translations
// in the target language from the bi-lingual dictionary which
// is an external resource. Let t(s) be the set of possible
// translations of a word s. For every source language topic z^S_k and
// target language topic z^T_k' compute
// \sum_s P(s|z^S_k) \sum_t(s) P(t(s)|z^T_k') 
//
// Return NULL on error
static float** computeTopicAlignmentProbs(LDAModel* src, LDAModel* tgt) {
	int     i, j;
	int     k, kprime;
	float** jd;
	ConMap  *e;
	WordTranslation *t;
	float    p, p_tgt_con_z, p_src_z;

	jd = (float**) malloc (sizeof(float*) * src->K);
	if (!jd) return NULL;
	for (i = 0; i < src->K; i++) {
		jd[i] = (float*) malloc (sizeof(float) * tgt->K);
		if (!jd[i]) return NULL;
	}

	// for every source topic
	for (k = 0; k < src->K; k++) {
		// for every target topic
		for (kprime = 0; kprime < tgt->K; kprime++) {
			p = 0;
			// Iterate upon every source word
			for (i = 0; i < src->V; i++) {
				// get the word to look up hash table of word translations				
				if (UNDEF == wid_to_word(src, i, &e)) {
					fprintf(stderr, "No word found for word index %d\n", i);
					continue;
				}
				// Query the bi-lingual dictionary to get the translations for
				// this particular word.
				if (UNDEF == text_bdict(e->word, &t)) {
					// this word does not belong to the dictionary. skip it...
					continue;
				}
				// Now iterate for all possible translations of this word.
				for (j = 1; j < t->numTranslations; j++) {
					p_tgt_con_z = p_con_z(tgt, kprime, t->cons[j]);
					p_src_z = p_w_z(src, k, i);
					p += p_src_z * p_tgt_con_z; 
				}
			}
			jd[k][kprime] = p / (float)(src->K);
		}
	}
	return jd;
}

/* This function computes P(w|Q) which is the approximation of P(w|R).
 * Do an LDA estimation over the top R pseudo relevant documents, and use
 * the probabilities P(w|z).P(z|d) marginalized over all topics. 
 * Accumulate the probabilities P(w|Q) for every word w
 * encountered while reading in the top documents. */
static int doSrcTgtLDA(VEC* qvec)
{
	TR_VEC  tr, src_tr;
	int     i;
	float   **topic_alignment_probs = NULL;

    tr.qid = qvec->id_num.id;
    if (UNDEF == seek_tr_vec(tr_fd, &tr) ||
		UNDEF == read_tr_vec(tr_fd, &tr))
		return(UNDEF);

    src_tr.qid = qvec->id_num.id;
    if (UNDEF == seek_tr_vec(src_tr_fd, &src_tr) ||
		UNDEF == read_tr_vec(src_tr_fd, &src_tr))
		return(UNDEF);

	/* Call the LDA estimator to estimate P(w|z) and P(z|d)s.
	 * This is LDA estimation for the target language. */
	if (UNDEF == init_lda_est_inst(&ip.target_ldamodel, doc_fd, &tr, qvec, 0)) {
	   	return (UNDEF);
	}
	lda_est(&ip.target_ldamodel);

	// The computation of P(w|Q) for each word in the target language
	// here needs to involve the LDA estimated from the source language documents
	// as well. To do this, load the LDA model previously saved
	// in a monolingual TRLM run for the source language.
	if (UNDEF == init_lda_est_ref_inst(&ip.src_ldamodel, ip.srcmodel_name, qvec)) {
	   	return (UNDEF);
	}

	// Compute the probabilities of alignment of topic from
	// a target model with a topic in the source language model.
	// P(z^B|z^E)
	topic_alignment_probs = computeTopicAlignmentProbs(&ip.src_ldamodel, &ip.target_ldamodel);
	if (!topic_alignment_probs)
		return UNDEF;
	// P(w^s|w^T)
	if ( UNDEF == computeWordAlignmentProbs(&ip.src_ldamodel, &ip.target_ldamodel, topic_alignment_probs)) {
		set_error(3, "out of memory", "computeWordAlignmentProbs"); 
		return UNDEF;
	}

	close_lda_est_inst(&ip.target_ldamodel);

	for (i = 0; i < ip.src_ldamodel.K; i++) {
		free(topic_alignment_probs[i]);
	}
	free(topic_alignment_probs);

	close_lda_est_inst(&ip.src_ldamodel);

	return 0;
}

static int comp_wordpair_corr (WordCorrelation *ptr1, WordCorrelation *ptr2) {
	return ptr1->corr < ptr2->corr? 1 : ptr1->corr == ptr2->corr? 0 : -1;
}

