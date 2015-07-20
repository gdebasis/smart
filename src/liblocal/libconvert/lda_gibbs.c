/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 * C library adapted from GibbsLDA++ for LDA estimation of the
 * pseudo-relevant documents
 * init_lda_est (SPEC *spec, int doc_fd, TR_VEC* tr_vec)
 * lda_est ()
 * close_lda_est ()
***********************************************************************/
#define _GNU_SOURCE

#include <math.h>
#include <time.h>
#include <ctype.h>
#include <search.h>
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
#include "tr_vec.h"
#include "textline.h"
#include "dict.h"
#include "docdesc.h"
#include "lda.h"

#define MAX_VOCAB_SIZE 524288

static int contok_inst;
static char *dict_file;
static long dict_mode;

static int sampling(LDAModel* ip, int m, int n);
int getWordMap(LDAModel* ip, CON_WT* conwtp);
static int addDoc(LDAModel* ip, VEC *vec, long* rel_count);
extern void bilda_addDoc_src(LDAModel* ip, int dict_fd, VEC *vec, long* rel_count);
int save_model(LDAModel* ip);
static int comp_topic_word (TopicWord* ptr1, TopicWord* ptr2);
static int save_model_twords(LDAModel* ip, char* filename);
static int save_model_params(LDAModel* ip, char* filename);
static int load_model(LDAModel* ip);
static int save_model_tassign(LDAModel* ip, char* filename);
int save_model_theta(LDAModel* ip, char* filename);
int save_model_phi(LDAModel* ip, char* filename);

static TopicWord* topicWords;

// Assumption: the vec passed as a parameter is a raw term frequency vector
static int getDocumentLength(VEC* vec) {
	long i, length = 0 ;
	CON_WT* start = vec->con_wtp ;

	// Read ctype_len[ctype] weights and add them up.
	for (i = 0; i < vec->ctype_len[0]; i++) {
		length += start[i].wt ;
	}
	return length ;
}

ConMap* getNextBuffEntry(LDAModel* ip) {
	ConMap* e;
	if (ip->conmap_buffptr >= ip->buff + ip->buffSize) {
		ip->buff = (ConMap*) realloc (ip->buff, ip->buffSize+5000);
		if (!ip->buff)
			return NULL;
		ip->buffSize += 5000;
	}
	e = ip->conmap_buffptr++;
	return e;
}

static int addDoc(LDAModel* ip, VEC *vec, long* rel_count) {
	CON_WT *conwtp;
	long   key;
	int    doclen;
	int    i, j;
	long   con;
	ConMap *e, *ep = NULL;
	Document* pdocs;
	char   *token;

	// Form the document object from dvec to be used by the LDA estimator
	pdocs = &ip->docs[*rel_count];
	// Find the length of this document
	doclen = getDocumentLength(vec);
	pdocs->words = (int*) malloc(sizeof(int) * doclen);
	pdocs->cons = (long*) malloc(sizeof(long) * doclen);
	pdocs->length = 0;

	// A hash table is maintained for counting the number of unique words
    for (i = 0, conwtp = vec->con_wtp; i < vec->ctype_len[0]; i++, conwtp++) {
		key = conwtp->con;
		HASH_FIND(hh, ip->conmap, &key, sizeof(long), ep);
		if (ep == NULL) {
			// a new word seen.. insert it in the hash
			e = getNextBuffEntry(ip);
			if (!e)
				return UNDEF;

			e->con = key;
			e->index = ip->V;
			if (UNDEF == ip->doc_desc.ctypes[0].con_to_token->proc (&key, &token, contok_inst)) {
			    token = "Not in dictionary";
		        clr_err();
			}
			snprintf(e->word, sizeof(e->word), "%s", token);
			//printf("Got word %s in doc %d\n", token, vec->id_num.id);

			ip->V++;
			HASH_ADD(hh, ip->conmap, con, sizeof(long), e);
			HASH_ADD(hh2, ip->widmap, index, sizeof(int), e);
			ep = e;
		}

		// A word has to be repeated tf times. Note: LDA assumes the vector
		// representation and hence word order is not important. 
		for (j = 0; j < (int)(conwtp->wt); j++) {
			pdocs->words[pdocs->length] = ep->index;  // store the word index
			pdocs->cons[pdocs->length] = ep->con; // store concept number
			pdocs->length++;
		}
   	}
	*rel_count += 1;
	return 0;
}

static int getNumRelDocs(TR_VEC* tr_vec, int rel_only) {
	int tr_ind, numRel = 0;
	for (tr_ind = 0; tr_ind < tr_vec->num_tr; tr_ind++) {
		if (rel_only) {
			if (tr_vec->tr[tr_ind].rel)
				numRel++;
		}
		else
			numRel++;
	}
	return numRel;
}

/* Construct the matrix of word ids (the first word encountered
 * in a document has an id of 0 and so on). Read each document
 * in turn from the tr_vec and incrementally build this matrix */
int constructDataset(LDAModel* ip) {
	int tr_ind;
	VEC dvec;
	long   rel_count = 0;
	int status;

	ip->V = 0;		// initialize the vocab size to 0 for this set of documents
	if (ip->multifaceted) ip->M++;
	ip->docs = (Document*) malloc(sizeof(Document) * ip->M);

	// Iterate for every document...
	for (tr_ind = 0; tr_ind < ip->tr_vec->num_tr; tr_ind++) {
		if (ip->rel_only) {
			if (ip->tr_vec->tr[tr_ind].rel == 0)
				continue;		// skip non-relevant documents
		}

		/* Open the document vector */
	    dvec.id_num.id = ip->tr_vec->tr[tr_ind].did;
	    if (UNDEF == seek_vector (ip->doc_fd, &dvec) ||
	        UNDEF == read_vector (ip->doc_fd, &dvec)) {
    	    return UNDEF;
	    }

		// Form the document object from dvec to be used by the LDA estimator
		status = addDoc(ip, &dvec, &rel_count);
		if (status == UNDEF)
			return UNDEF;
    }

	if (ip->multifaceted) {
		// For an m-TRLM, add the query in the LDA space
		status = addDoc(ip, ip->qvec, &rel_count);
		if (status == UNDEF)
			return UNDEF;
	}
	return 0;
}

void freeMatrixInt(int** p, int nr) {
	int i;
	for (i = 0; i < nr; i++)
		free(p[i]);
	free(p);
}

void freeMatrixDouble(float** p, int nr) {
	int i;
	for (i = 0; i < nr; i++)
		free(p[i]);
	free(p);
}

/* Computes P(w,z_k,phi) */ 
float getProbWordFromTopic(LDAModel* model, CON_WT* conwtp, int k) {
	int wordId = getWordMap(model, conwtp);
	return p_w_z(model, k, wordId);
}

// Returns the integer id associated with the given word used in LDA estimation.
// which is the ordinal index of a concept number.
int getWordMap(LDAModel* ip, CON_WT* conwtp) {
	ConMap *found_conmap;
	long   key;
   
	key	= conwtp->con;
	HASH_FIND(hh, ip->conmap, &key, sizeof(long), found_conmap);
	if (!found_conmap)
		return UNDEF;
	return found_conmap->index;
}

int con_to_wid(LDAModel* ip, long con) {
	ConMap *found_conmap;
	HASH_FIND(hh, ip->conmap, &con, sizeof(long), found_conmap);
	if (!found_conmap)
		return UNDEF;
	return found_conmap->index;
}

long wid_to_con(LDAModel* ip, int wid) {
	ConMap *found_conmap;

	// Get the concept number from the word index
	HASH_FIND(hh2, ip->widmap, &wid, sizeof(int),  found_conmap);
	if (!found_conmap)
		return UNDEF;
	return found_conmap->con;
}

int wid_to_word(LDAModel* ip, int wid, ConMap** conmap) {
	// Get the concept number from the word index
	HASH_FIND(hh2, ip->widmap, &wid, sizeof(int), (*conmap));
	if (!*conmap)
		return UNDEF;
	return 0;
}

/* Compute the marginalized probabilities of generating the query qvec
 * from each top ranked pseudo-relevant document */ 
float getMarginalizedQueryGenEstimate(LDAModel* ip, int docid, float* collstats_freq, long totalDocFreq) {
	CON_WT *con_wtp;
	int qword_id, z;
	float p_q_z;
	float p_Q_D = 0;  // P(Q|D): probability of generating the entire query from the current document
	float p_q_D;      // P(q_i|D): probability of generating a single query term from the current document

	for (con_wtp = ip->qvec->con_wtp; con_wtp < &ip->qvec->con_wtp[ip->qvec->ctype_len[0]]; con_wtp++) {
		p_q_D = 0;
		qword_id = getWordMap(ip, con_wtp);
		for (z = 0; z < ip->K; z++) {
			// if the query term is not present in the pseudo-relevant
			// set of documents, add the MLE of generating that
			// query term from the collection
			if (collstats_freq)
				p_q_z = (qword_id == UNDEF)? collstats_freq[con_wtp->con]/(float)totalDocFreq : p_w_z(ip, z, qword_id);
			else		   		
				p_q_z = p_w_z(ip, z, qword_id);
			p_q_D += p_q_z * p_z_d(ip, z, docid);
		}
		p_Q_D += log(1+ p_q_D);
	}
	return p_Q_D;
}

float compute_marginalized_P_w_D(LDAModel* ip, CON_WT* conwtp, int docIndex) {
	// Return a linear combination (smoothed) marginalized maximum likelihoods and collection prob.
	// for the given word.
	float lda = 0;
	int   i, wordId;

	wordId = getWordMap(ip, conwtp);
	for (i = 0; i < ip->K; i++) {
		///REVISIT:
		lda += p_w_z(ip, i, wordId) * p_z_d(ip, i, docIndex);
		///lda += log(1 + p_w_z(ip, i, wordId) + p_z_d(ip, i, docIndex));
	}
	return lda;
}

float compute_single_topic_P_w_D(LDAModel* ip, CON_WT* conwtp, int docIndex, int topicIndex) {
	// Return a linear combination (smoothed) marginalized maximum likelihoods and collection prob.
	// for the given word for a given topic.
	float lda = 0;
	int   wordId;

	wordId = getWordMap(ip, conwtp);
	lda = p_w_z(ip, topicIndex, wordId) * p_z_d(ip, topicIndex, docIndex);
	return lda;
}

float per_topic_p_w_d(LDAModel* ip, CON_WT* conwtp, int docIndex) {
	// Return a linear combination of maximum likelihoods and collection prob.
	// for the given word for a single topic (the maximum likelihood one) only...
	float lda = 0, p_w_z_max = 0, p;
	int   i, wordId, k_max = 0;

	wordId = getWordMap(ip, conwtp);
	for (i = 0; i < ip->K; i++) {
		p = p_w_z(ip, i, wordId);
		if (p > p_w_z_max) {
			p_w_z_max = p;
			k_max = i;
		}
	}

	lda = p_w_z(ip, k_max, wordId) * p_z_d(ip, k_max, docIndex);
	return lda;
}

/* Return the probability of generating the given word 'w' from the given topic 'topicId' */
float p_con_z(LDAModel* ip, int topicId, long con) {
	int wordId = con_to_wid(ip, con);
	if (UNDEF == wordId) {
		return 0;
	}
		
	if (!(wordId >= 0 && wordId < ip->V))
		return 0;
	if (!(topicId >= 0 && topicId < ip->K))
		return 0;

	return ip->phi[topicId][wordId];
}

/* Return the probability of generating the given word 'w' from the given topic 'topicId' */
float p_w_z(LDAModel* ip, int topicId, int wordId) {
	if (!(wordId >= 0 && wordId < ip->V))
		return 0;
	if (!(topicId >= 0 && topicId < ip->K))
		return 0;

	return ip->phi[topicId][wordId];
}

/* Return the probability of generating the given word topic from the given document */
float p_z_d(LDAModel* ip, int topicId, int docIndex) {
	if (!(docIndex >= 0 && docIndex < ip->M))
		return 0;
	if (!(topicId >= 0 && topicId < ip->K))
		return 0;

	return ip->theta[docIndex][topicId];
}

// initialize once
int init_lda_est(LDAModel* ip, SPEC* spec) {
	SPEC_PARAM spec_args[] = {
    	{"lda_est.dict_file",  getspec_dbfile, (char *) &dict_file},
    	{"lda_est.dict_file.rmode",  getspec_filemode, (char *) &dict_mode},
    	{"lda_est.num_topics", getspec_int, (char *) &ip->K},
	    {"lda_est.alpha", getspec_float, (char *) &ip->alpha},
    	{"lda_est.beta", getspec_float, (char *) &ip->beta},
	    {"lda_est.iters", getspec_int, (char *) &ip->niters},
    	{"lda_est.model_name_prefix", getspec_string, (char *) &ip->model_name_prefix},
	    {"lda_est.twords", getspec_int, (char *) &ip->twords},
    	{"lda_est.tosave", getspec_bool, (char *) &ip->tosave},
    	{"lda_est.rel_only", getspec_bool, (char *) &ip->rel_only},
    	//{"lda_est.vocab_size", getspec_int, (char *) &ip->buffSize},
    	TRACE_PARAM ("lda_est.trace")
	}; 
	int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

    PRINT_TRACE (2, print_string, "Trace: entering init_lda_est");

	ip->buffSize = MAX_VOCAB_SIZE;
	ip->buff = (ConMap*) malloc (sizeof(ConMap) * ip->buffSize);
	ip->conmap_buffptr = ip->buff;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return UNDEF;
    if (UNDEF == lookup_spec_docdesc (spec, &ip->doc_desc))
        return (UNDEF);

    /* Reserve space for the instantiation ids of the called procedures. */
    if (UNDEF == (contok_inst = ip->doc_desc.ctypes[0].con_to_token->init_proc(spec, "index.ctype.0.")))
		return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_lda_est");
}

// This function is called from the BiLDA component to initialize the base class members...
int init_bilda_est_inst_base (BiLDAModel* bilda, int doc_fd, TR_VEC* tr_vec, VEC* qvec) {
	int m, n, w, k, tr_ind;
	int N, topic;
	VEC dvec;
	LDAModel *ip = &(bilda->ldaModel);
	long rel_count = 0;

    PRINT_TRACE (2, print_string, "Trace: entering init_bilda_est_inst_base");
	ip->conmap_buffptr = ip->buff;	// reset buffer

	ip->doc_fd = doc_fd;
	ip->tr_vec = tr_vec;
	ip->qvec = qvec;
	ip->multifaceted = 0;
	ip->conmap = NULL;
	ip->widmap = NULL;

	ip->M = getNumRelDocs(tr_vec, ip->rel_only);
	if (ip->M == 0)
		return UNDEF;
    PRINT_TRACE (2, print_string, "Trace: M:");
    PRINT_TRACE (4, print_long, ip->M);

	ip->p = (float*) Malloc(ip->K, float);
	if (ip->alpha == 0)
		ip->alpha = 50/(float)ip->K;	// override the default. alpha = 50/K


	ip->V = 0;		// initialize the vocab size to 0 for this set of documents
	ip->multifaceted = 0;
	ip->docs = (Document*) malloc(sizeof(Document) * ip->M);

	// Iterate for every document...
	for (tr_ind = 0; tr_ind < ip->tr_vec->num_tr; tr_ind++) {
		if (ip->rel_only) {
			if (ip->tr_vec->tr[tr_ind].rel == 0)
				continue;		// skip non-relevant documents
		}

		/* Open the document vector */
	    dvec.id_num.id = ip->tr_vec->tr[tr_ind].did;
	    if (UNDEF == seek_vector (ip->doc_fd, &dvec) ||
	        UNDEF == read_vector (ip->doc_fd, &dvec)) {
    	    return UNDEF;
	    }

		// Form the document object from dvec to be used by the LDA estimator
		bilda_addDoc_src(&(bilda->ldaModel), bilda->src_dict_fd, &dvec, &rel_count);
    }

    // K: from command line or default value
    // alpha, beta: from command line or default values
    // niters, savestep: from command line or default values
	// nw is a VxK matrix
    ip->nw = (int**) calloc (ip->V, sizeof(int*));
    for (w = 0; w < ip->V; w++) {
        ip->nw[w] = (int*) calloc (ip->K, sizeof(int));
    }
	
    ip->nd = (int**) calloc (ip->M, sizeof(int*));
    for (m = 0; m < ip->M; m++) {
        ip->nd[m] = (int*) calloc (ip->K, sizeof(int));
    }
	
    ip->nwsum = (int*) calloc (ip->K, sizeof(int));
    ip->ndsum = (int*) calloc (ip->M, sizeof(int));;

    srandom(time(0)); // initialize for random number generation
    ip->z = (int**) calloc (ip->M, sizeof(int*));

    for (m = 0; m < ip->M; m++) {
		N = ip->docs[m].length;
    	ip->z[m] = (int*) calloc (N, sizeof(int));;
	
        // initialize for z
        for (n = 0; n < N; n++) {
    	    topic = rand() % ip->K;
    	    ip->z[m][n] = topic;
    	  
		   	if (!(topic < ip->K && ip->docs[m].words[n] < ip->V)) {
				fprintf(stderr, "Error: topic=%d, K=%d; word[%d]=%d,V=%d\n",
							topic, ip->K, n, ip->docs[m].words[n], ip->V);
				return UNDEF;
			}
    	    // number of instances of word i assigned to topic j
    	    ip->nw[ip->docs[m].words[n]][topic] += 1;
    	    // number of words in document i assigned to topic j
    	    ip->nd[m][topic] += 1;
    	    // total number of words assigned to topic j
    	    ip->nwsum[topic] += 1;
        } 
        // total number of words in document i
        ip->ndsum[m] = N;      
    }
    
    ip->theta = (float**) calloc (ip->M, sizeof(float*));
    for (m = 0; m < ip->M; m++) {
        ip->theta[m] = (float*) calloc(ip->K, sizeof(float));
    }
	
    ip->phi = (float**) calloc (ip->K, sizeof(float*));
    for (k = 0; k < ip->K; k++) {
        ip->phi[k] = (float*) calloc(ip->V, sizeof(float));
    }    
    
    PRINT_TRACE (2, print_string, "Trace: leaving init_bilda_est_inst_base");
	return 0;
}

int init_lda_est_inst (LDAModel* ip, int doc_fd, TR_VEC* tr_vec, VEC* qvec, int multifaceted) {
	int m, n, w, k;
	int N, topic;
	int status;

    PRINT_TRACE (2, print_string, "Trace: entering init_lda_est_inst");

	ip->conmap_buffptr = ip->buff;	// reset buffer

	ip->doc_fd = doc_fd;
	ip->tr_vec = tr_vec;
	ip->qvec = qvec;
	ip->multifaceted = multifaceted;
	ip->conmap = NULL;
	ip->widmap = NULL;

	ip->M = getNumRelDocs(tr_vec, ip->rel_only);
	if (ip->M == 0)
		return UNDEF;

	ip->p = (float*) Malloc(ip->K, float);
	if (ip->alpha == 0)
		ip->alpha = 50/(float)ip->K;	// override the default. alpha = 50/K

	status = constructDataset(ip);
	if (status == UNDEF)
		return UNDEF;

    // K: from command line or default value
    // alpha, beta: from command line or default values
    // niters, savestep: from command line or default values
	// nw is a VxK matrix
    ip->nw = (int**) calloc (ip->V, sizeof(int*));
    for (w = 0; w < ip->V; w++) {
        ip->nw[w] = (int*) calloc (ip->K, sizeof(int));
    }
	
    ip->nd = (int**) calloc (ip->M, sizeof(int*));
    for (m = 0; m < ip->M; m++) {
        ip->nd[m] = (int*) calloc (ip->K, sizeof(int));
    }
	
    ip->nwsum = (int*) calloc (ip->K, sizeof(int));
    ip->ndsum = (int*) calloc (ip->M, sizeof(int));;

    srandom(time(0)); // initialize for random number generation
    ip->z = (int**) calloc (ip->M, sizeof(int*));

    for (m = 0; m < ip->M; m++) {
		N = ip->docs[m].length;
    	ip->z[m] = (int*) calloc (N, sizeof(int));;
	
        // initialize for z
        for (n = 0; n < N; n++) {
    	    topic = rand() % ip->K;
    	    ip->z[m][n] = topic;
    	  
		   	if (!(topic < ip->K && ip->docs[m].words[n] < ip->V)) {
				fprintf(stderr, "Error: topic=%d, K=%d; word[%d]=%d,V=%d\n",
							topic, ip->K, n, ip->docs[m].words[n], ip->V);
				return UNDEF;
			}
    	    // number of instances of word i assigned to topic j
    	    ip->nw[ip->docs[m].words[n]][topic] += 1;
    	    // number of words in document i assigned to topic j
    	    ip->nd[m][topic] += 1;
    	    // total number of words assigned to topic j
    	    ip->nwsum[topic] += 1;
        } 
        // total number of words in document i
        ip->ndsum[m] = N;      
    }
    
    ip->theta = (float**) calloc (ip->M, sizeof(float*));
    for (m = 0; m < ip->M; m++) {
        ip->theta[m] = (float*) calloc(ip->K, sizeof(float));
    }
	
    ip->phi = (float**) calloc (ip->K, sizeof(float*));
    for (k = 0; k < ip->K; k++) {
        ip->phi[k] = (float*) calloc(ip->V, sizeof(float));
    }    
    
    PRINT_TRACE (2, print_string, "Trace: leaving init_lda_est_inst");
    return 0;
}

static void constructDatasetFromCollection(LDAModel* ip) {
	VEC dvec;
	long rel_count = 0;

	ip->V = 0;		// initialize the vocab size to 0 for this set of documents
	ip->docs = (Document*) malloc(sizeof(Document) * ip->M);

    /* Get each document in turn */
    while (1 == read_vector (ip->doc_fd, &dvec)) {
		// Form the document object from dvec to be used by the LDA estimator
		addDoc(ip, &dvec, &rel_count);
		PRINT_TRACE (4, print_vector, &dvec);
    }
}

// Init LDA inference on the whole collection instead of top ranked documents
int init_lda_est_inst_coll (LDAModel* ip, int doc_fd, int M) {
	int m, n, w, k;
	int N, topic;

    PRINT_TRACE (2, print_string, "Trace: entering init_lda_est_inst");

	ip->conmap_buffptr = ip->buff;	// reset buffer

	ip->M = M;
	ip->doc_fd = doc_fd;
	ip->tr_vec = NULL;
	ip->qvec = NULL;
	ip->multifaceted = 0;
	ip->conmap = NULL;
	ip->widmap = NULL;

	ip->p = (float*) Malloc(ip->K, float);
	if (!ip->p) {
		fprintf(stderr, "Unable to allocate memory...");
		return UNDEF;
	}

	if (ip->alpha == 0)
		ip->alpha = 50/(float)ip->K;	// override the default. alpha = 50/K

    PRINT_TRACE (2, print_string, "Trace: Loading documents in memory");
	constructDatasetFromCollection(ip);
    PRINT_TRACE (2, print_string, "Trace: Loaded documents in memory");

    // K: from command line or default value
    // alpha, beta: from command line or default values
    // niters, savestep: from command line or default values
	// nw is a VxK matrix
    ip->nw = (int**) calloc (ip->V, sizeof(int*));
	if (!ip->nw) {
		fprintf(stderr, "Unable to allocate memory...");
		return UNDEF;
	}
    for (w = 0; w < ip->V; w++) {
        ip->nw[w] = (int*) calloc (ip->K, sizeof(int));
		if (!ip->nw[w]) {
			fprintf(stderr, "Unable to allocate memory...");
			return UNDEF;
		}
    }
	
    ip->nd = (int**) calloc (ip->M, sizeof(int*));
	if (!ip->nd) {
		fprintf(stderr, "Unable to allocate memory...");
		return UNDEF;
	}
    for (m = 0; m < ip->M; m++) {
        ip->nd[m] = (int*) calloc (ip->K, sizeof(int));
		if (!ip->nd[m]) {
			fprintf(stderr, "Unable to allocate memory...");
			return UNDEF;
		}
    }
	
    ip->nwsum = (int*) calloc (ip->K, sizeof(int));
	if (!ip->nwsum) {
		fprintf(stderr, "Unable to allocate memory...");
		return UNDEF;
	}
    ip->ndsum = (int*) calloc (ip->M, sizeof(int));;
	if (!ip->ndsum) {
		fprintf(stderr, "Unable to allocate memory...");
		return UNDEF;
	}

    srandom(time(0)); // initialize for random number generation
    ip->z = (int**) calloc (ip->M, sizeof(int*));
	if (!ip->z) {
		fprintf(stderr, "Unable to allocate memory...");
		return UNDEF;
	}

    for (m = 0; m < ip->M; m++) {
		N = ip->docs[m].length;
    	ip->z[m] = (int*) calloc (N, sizeof(int));;
		if (!ip->z[m]) {
			fprintf(stderr, "Unable to allocate memory...");
			return UNDEF;
		}
	
        // initialize for z
        for (n = 0; n < N; n++) {
    	    topic = rand() % ip->K;
    	    ip->z[m][n] = topic;
    
		   	if (!(topic < ip->K && ip->docs[m].words[n] < ip->V)) {
				fprintf(stderr, "Error: topic=%d, K=%d; word[%d]=%d,V=%d\n",
							topic, ip->K, n, ip->docs[m].words[n], ip->V);
				return UNDEF;
			}
    	    // number of instances of word i assigned to topic j
    	    ip->nw[ip->docs[m].words[n]][topic] += 1;
    	    // number of words in document i assigned to topic j
    	    ip->nd[m][topic] += 1;
    	    // total number of words assigned to topic j
    	    ip->nwsum[topic] += 1;
        } 
        // total number of words in document i
        ip->ndsum[m] = N;      
    }
    
    ip->theta = (float**) calloc (ip->M, sizeof(float*));
    for (m = 0; m < ip->M; m++) {
        ip->theta[m] = (float*) calloc(ip->K, sizeof(float));
		if (!ip->theta[m]) {
			fprintf(stderr, "Unable to allocate memory...");
			return UNDEF;
		}
    }
	
    ip->phi = (float**) calloc (ip->K, sizeof(float*));
    for (k = 0; k < ip->K; k++) {
        ip->phi[k] = (float*) calloc(ip->V, sizeof(float));
		if (!ip->phi[k]) {
			fprintf(stderr, "Unable to allocate memory...");
			return UNDEF;
		}
    }    
    
    PRINT_TRACE (2, print_string, "Trace: leaving init_lda_est_inst");
    return 0;
}

int close_lda_est (LDAModel* ip) {
    PRINT_TRACE (2, print_string, "Trace: entering close_lda_est");

    if (UNDEF == ip->doc_desc.ctypes[0].con_to_token->close_proc(contok_inst))
		return (UNDEF);

	free(ip->buff);

    PRINT_TRACE (2, print_string, "Trace: leaving close_lda_est");
}

int close_lda_est_inst (LDAModel* ip)
{
	int i;
	Document *pdoc;

    PRINT_TRACE (2, print_string, "Trace: entering close_lda_est_inst");
	freeMatrixInt(ip->nw, ip->V);
	freeMatrixInt(ip->nd, ip->M);
	free(ip->nwsum);
	free(ip->ndsum);
	free(ip->p);

	freeMatrixInt(ip->z, ip->M);
	freeMatrixDouble(ip->theta, ip->M);
	freeMatrixDouble(ip->phi, ip->K);

	for (i = 0; i < ip->M; i++) {
		pdoc = ip->docs + i;
		free(pdoc->words);
		free(pdoc->cons);
	}
	free(ip->docs);

	HASH_CLEAR(hh, ip->conmap);
	HASH_CLEAR(hh2, ip->widmap);

    PRINT_TRACE (2, print_string, "Trace: leaving close_lda_est_inst");
    return 0;
}

int lda_est(LDAModel* ip) {
	static char msg[256];
	int iter;
	int m, n, topic;
	double p_w_z_posterior;

    PRINT_TRACE (2, print_string, "Trace: entering lda_est");

    snprintf(msg, sizeof(msg), "Sampling %d iterations!\n", ip->niters);
	PRINT_TRACE(4, print_string, msg);

    for (iter = 0; iter <= ip->niters; iter++) {
		// for all z_i
		for (m = 0; m < ip->M; m++) {
	    	for (n = 0; n < ip->docs[m].length; n++) {
				// (z_i = z[m][n])
				// sample from p(z_i|z_-i, w)
				topic = sampling(ip, m, n);
				ip->z[m][n] = topic;
	    	}
		}
    	snprintf(msg, sizeof(msg), "Gibbs sampling %d\n", iter);
		PRINT_TRACE(2, print_string, msg);
    }
   
	if (ip->tr_vec) {	
    	snprintf(msg, sizeof(msg), "Gibbs sampling completed for topic %d", ip->tr_vec->qid);
		PRINT_TRACE(4, print_string, msg);
    	snprintf(msg, sizeof(msg), "Saving the final model for topic %d", ip->tr_vec->qid);
		PRINT_TRACE(4, print_string, msg);
	}

    compute_theta(ip);
    compute_phi(ip);

	// Save the model for future use.
	if (ip->tosave) {
	    if (save_model(ip) == UNDEF)
			return UNDEF;
	}

#if 0
	p_w_z_posterior = compute_posterior(ip);
	fprintf(stdout, "%d %f\n", ip->tr_vec->qid, p_w_z_posterior); 
#endif

	return 0;
}

static int lda_est_resampling(LDAModel* ip) {
	static char msg[256];
	int iter;
	int m, n, topic;

    PRINT_TRACE (2, print_string, "Trace: entering lda_est");

    snprintf(msg, sizeof(msg), "Sampling %d iterations!\n", ip->niters);
	PRINT_TRACE(4, print_string, msg);

	//REVISIT: Introduce a variable for handling the number of resamplings required.
	// For the time being we don't need to run any more iterations...
    for (iter = ip->niters+1; iter <= ip->niters; iter++) {
		// for all z_i
		for (m = 0; m < ip->M; m++) {
	    	for (n = 0; n < ip->docs[m].length; n++) {
				// (z_i = z[m][n])
				// sample from p(z_i|z_-i, w)
				topic = sampling(ip, m, n);
				ip->z[m][n] = topic;
	    	}
		}
    }
   
	if (ip->tr_vec) {	
    	snprintf(msg, sizeof(msg), "Gibbs sampling completed for topic %d", ip->tr_vec->qid);
		PRINT_TRACE(4, print_string, msg);
    	snprintf(msg, sizeof(msg), "Saving the final model for topic %d", ip->tr_vec->qid);
		PRINT_TRACE(4, print_string, msg);
	}

    compute_theta(ip);
    compute_phi(ip);

	return 0;
}

static int sampling(LDAModel* ip, int m, int n) {
    // remove z_i from the count variables
    int topic = ip->z[m][n];
    int w = ip->docs[m].words[n];
	int k;
	float u;

    ip->nw[w][topic] -= 1;
    ip->nd[m][topic] -= 1;
    ip->nwsum[topic] -= 1;
    ip->ndsum[m] -= 1;

    float Vbeta = ip->V * ip->beta;
    float Kalpha = ip->K * ip->alpha;    
    // do multinomial sampling via cumulative method
    for (k = 0; k < ip->K; k++) {
		ip->p[k] = (ip->nw[w][k] + ip->beta) / (ip->nwsum[k] + Vbeta) *
		    (ip->nd[m][k] + ip->alpha) / (ip->ndsum[m] + Kalpha);
    }
    
	// cumulate multinomial parameters
    for (k = 1; k < ip->K; k++) {
		ip->p[k] += ip->p[k - 1];
    }

    // scaled sample because of unnormalized p[]
    u = ((float)random() / (float)RAND_MAX) * ip->p[ip->K - 1];
    
    for (topic = 0; topic < ip->K-1; topic++) {
		if (ip->p[topic] > u) {
	    	break;
		}
    }
    
    // add newly estimated z_i to count variables
    ip->nw[w][topic] += 1;
    ip->nd[m][topic] += 1;
    ip->nwsum[topic] += 1;
    ip->ndsum[m] += 1;    
    
    return topic;
}

void compute_theta(LDAModel* ip) {
	int m, k;

    for (m = 0; m < ip->M; m++) {
		for (k = 0; k < ip->K; k++) {
	    	ip->theta[m][k] = (ip->nd[m][k] + ip->alpha) / (ip->ndsum[m] + ip->K * ip->alpha);
		}
    }
}

void compute_phi(LDAModel* ip) {
	int k, w;
    for (k = 0; k < ip->K; k++) {
		for (w = 0; w < ip->V; w++) {
		    ip->phi[k][w] = (ip->nw[w][k] + ip->beta) / (ip->nwsum[k] + ip->V * ip->beta);
		}
    }
}

// This can be used to compute the number of desired topics.
// In effect we compute the log of the posterior since the numbers
// can overflow or underflow
double compute_posterior(LDAModel* ip) {
	int k, w;
	double beta_term;
	double w_term;
	double x, log_x;
	double sum = 0;

	beta_term = ip->K * (log(gamma(ip->V*ip->beta)) - ip->V * log(gamma(ip->beta)));
	
    for (k = 0; k < ip->K; k++) {
		w_term = 0;
		for (w = 0; w < ip->V; w++) {
			x = gamma(ip->nw[w][k] + ip->beta);
			log_x = log(1+x);
			w_term += log_x;
		}
		w_term -= log(gamma(ip->nwsum[k] + ip->V*ip->beta));
		sum += w_term;
	}

	return beta_term + sum;
}

int save_model(LDAModel* ip) {
	static char filename[256];
	int qid = ip->qvec? ip->qvec->id_num.id : 0;

	if (sm_trace >= 4) {
		snprintf(filename, sizeof(filename), "%s.%d.theta", ip->model_name_prefix, qid); 
    	if (save_model_theta(ip, filename) == UNDEF) {
	    	return UNDEF;
	    }

		snprintf(filename, sizeof(filename), "%s.%d.phi", ip->model_name_prefix, qid); 
	    if (save_model_phi(ip, filename) == UNDEF) {
    		return UNDEF;
	    }
	}

	snprintf(filename, sizeof(filename), "%s.%d.params", ip->model_name_prefix, qid);
	if (UNDEF == save_model_params(ip, filename))
		return UNDEF;

	if (ip->twords > 0) {
		snprintf(filename, sizeof(filename), "%s.%d.twords", ip->model_name_prefix, qid);
		if (UNDEF == save_model_twords(ip, filename))
			return UNDEF;
		snprintf(filename, sizeof(filename), "%s.%d.tassign", ip->model_name_prefix, qid); 
	    if (save_model_tassign(ip, filename) == UNDEF) {
		    return UNDEF;
	    }
	}

    return 0;
}

static int comp_topic_word (TopicWord* ptr1, TopicWord* ptr2) {
	return ptr1->phi < ptr2->phi? -1 : ptr1->phi == ptr2->phi? 0 : 1;
}

static int comp_topic_word_descending (TopicWord* ptr1, TopicWord* ptr2) {
	return -1 * comp_topic_word(ptr1, ptr2);
}

void getMostLikelyWords(LDAModel* ip, int k, TopicWord* topicWords, int numWords) {
	int w;
	ConMap *found_conmap;
	TopicWord *last = topicWords + numWords - 1;

	memset(topicWords, 0, sizeof(TopicWord)*numWords); 

    for (w = 0; w < ip->V; w++) {
		// Get the concept number from the word index
		HASH_FIND(hh2, ip->widmap, &w, sizeof(int),  found_conmap);
		if (!found_conmap)
			continue;
		if (ip->phi[k][w] <= last->phi)
			continue;
		
		last->con = found_conmap->con;
		last->w = w;
		last->phi = ip->phi[k][w];

		qsort(topicWords, numWords, sizeof(TopicWord), comp_topic_word_descending);
    }
}

// Print out the top N most likely words of a topic
static int save_model_twords(LDAModel* ip, char* filename) {
	int k, w, key;
	ConMap *found_conmap;
	TopicWord *twp;
	char *token;
	int   twords;
	float maxprob = 0;
	int   assigned_topic = 0;
	static wchar_t utfToken[256];

    FILE * fout = fopen(filename, "w");
    if (!fout) {
		fprintf(stderr, "Cannot open file %s to save!\n", filename);
		return UNDEF;
    }

	if (ip->twords > ip->V) {
		ip->twords = ip->V;
	}

#if 0	
	topicWords = (TopicWord*) malloc (sizeof(TopicWord) * ip->V);

    for (k = 0; k < ip->K; k++) {
		twp = topicWords;
	    for (w = 0; w < ip->V; w++) {
			key	= w;
			// Get the concept number from the word index
			HASH_FIND(hh2, ip->widmap, &key, sizeof(int),  found_conmap);
			if (!found_conmap)
				return UNDEF;
			twp->con = found_conmap->con;
			twp->w = w;
			twp->phi = ip->phi[k][w];
			twp++;
	    }
		// sort in ascending order of phi
		qsort(topicWords, twp - topicWords, sizeof(TopicWord), comp_topic_word);
		fprintf(fout, "Topic %d:\n", k);
		// print the top 'twords' phis
		for (twords = 0, twp = twp-1; twp >= topicWords && twords < ip->twords; twp--, twords++) {
			// Get the word string from the concept number
			if (UNDEF == doc_desc.ctypes[0].con_to_token->proc (&(twp->con), &token, contok_inst)) {
			    token = "Not in dictionary";
		        clr_err();
			}
			fprintf(fout, "%d %d %s %f\n", twp->w, twp->con, token, twp->phi);
		}
	}

	free(topicWords);
#endif

	topicWords = (TopicWord*) malloc (sizeof(TopicWord) * ip->V);
	for (w = 0, twp = topicWords; w < ip->V; w++, twp++) {
		maxprob = ip->phi[0][w];
		assigned_topic = 0;
    	for (k = 1; k < ip->K; k++) {
			if (ip->phi[k][w] > maxprob) {
				maxprob = ip->phi[k][w];
				assigned_topic = k;
			}
		}
		// Get the concept number from the word index
		key	= w;
		HASH_FIND(hh2, ip->widmap, &key, sizeof(int),  found_conmap);
		if (!found_conmap)
			return UNDEF;
		twp->con = found_conmap->con;
		twp->w = w;
		twp->phi = maxprob;
		twp->topic = assigned_topic;
	}

	qsort(topicWords, ip->V, sizeof(TopicWord), comp_topic_word_descending);

   	for (k = 0; k < ip->K; k++) {
		twords = 0;
		fprintf(fout, "Topic %d:\n", k);
		for (twp = topicWords; twp < &topicWords[ip->V]; twp++) {
			if (twords >= ip->twords)
				break;
			if (twp->topic == k) {
				// Get the word string from the concept number
				if (UNDEF == ip->doc_desc.ctypes[0].con_to_token->proc (&(twp->con), &token, contok_inst)) {
			    	token = "Not in dictionary";
		        	clr_err();
				}
				mbstowcs(utfToken, token, sizeof(utfToken)/sizeof(*utfToken));
				fprintf(fout, "%d %d %ls %f\n", twp->w, twp->con, utfToken, twp->phi);
				twords++;
			}
		}
	}

	fclose(fout);
    return 0;
}

static int save_model_params(LDAModel* ip, char* filename) {
    FILE * fout = fopen(filename, "w");
    if (!fout) {
		fprintf(stderr, "Cannot open file %s to save!\n", filename);
		return UNDEF;
    }

    fprintf(fout, "ntopics=%d ndocs=%d nwords=%d alpha=%f beta=%f",
					ip->K, ip->M, ip->V, ip->alpha, ip->beta);
    fclose(fout);    
    return 0;
}

int init_lda_est_ref_inst (LDAModel* ip, char* saved_model_name_prefix, VEC* qvec) {
	int m, n, w, k;
	int N, topic;

    PRINT_TRACE (2, print_string, "Trace: entering init_lda_est_ref_inst");
	
	ip->conmap_buffptr = ip->buff;

	// Note: Instead of reading in the parameters from a spec file, we 
	// get the LDA parameters from a saved LDA file. This model does not
	// require a name since we are not gonna save it again.
	ip->qvec = qvec;
	ip->model_name_prefix = saved_model_name_prefix;

	// Load the z array from the tassign file
	if (UNDEF == load_model(ip))
		return UNDEF;

	ip->p = (float*) Malloc(ip->K, float);
    ip->nw = (int**) calloc (ip->V, sizeof(int*));
    for (w = 0; w < ip->V; w++) {
        ip->nw[w] = (int*) calloc (ip->K, sizeof(int));
    }
    ip->nd = (int**) calloc (ip->M, sizeof(int*));
    for (m = 0; m < ip->M; m++) {
        ip->nd[m] = (int*) calloc (ip->K, sizeof(int));
    }
	
    ip->nwsum = (int*) calloc (ip->K, sizeof(int));
    ip->ndsum = (int*) calloc (ip->M, sizeof(int));;

    for (m = 0; m < ip->M; m++) {
		N = ip->docs[m].length;
	
        // initialize for z
        for (n = 0; n < N; n++) {
			w = ip->docs[m].words[n];
    	    topic = ip->z[m][n];
    	  
		   	if (!(topic < ip->K && ip->docs[m].words[n] < ip->V)) {
				fprintf(stderr, "Error: topic=%d, K=%d; word[%d]=%d,V=%d\n",
							topic, ip->K, n, ip->docs[m].words[n], ip->V);
				return UNDEF;
			}
    	    // number of instances of word i assigned to topic j
    	    ip->nw[w][topic] += 1;
    	    // number of words in document i assigned to topic j
    	    ip->nd[m][topic] += 1;
    	    // total number of words assigned to topic j
    	    ip->nwsum[topic] += 1;
        } 
        // total number of words in document i
        ip->ndsum[m] = N;      
    }
   
   	// Allocate space and assign values for theta and phi	
    ip->theta = (float**) calloc (ip->M, sizeof(float*));
    for (m = 0; m < ip->M; m++) {
        ip->theta[m] = (float*) calloc(ip->K, sizeof(float));
    }

    ip->phi = (float**) calloc (ip->K, sizeof(float*));
    for (k = 0; k < ip->K; k++) {
        ip->phi[k] = (float*) calloc(ip->V, sizeof(float));
    }    
  
	// Have to run the resampling process...   	
	if (UNDEF == lda_est_resampling(ip))
		return UNDEF;

    PRINT_TRACE (2, print_string, "Trace: leaving init_lda_est_ref_inst");
    return 0;
}

/* Load a previously saved estimated LDA model for the top ranked
 * documents of a given query.
 * This is particularly useful in aligning with topics in a
 * different language and thus forms the basis of TRLM
 * inference for CLIR. */
static int load_model(LDAModel* ip)
{
	static char filename[256];
    static char buff[MAX_VOCAB_SIZE<<1];
	FILE*       fin;
	SM_TEXTLINE textline;
	int         length;
	int         i, j;
	char        *line, *token, *word, *pos;
	Document    *pdoc;
	int 		qid;
	char**      tokens;
	ConMap      e, *ep = NULL;
	
	qid = ip->qvec->id_num.id;

	// Read parameters from params file
	snprintf(filename, sizeof(filename), "%s.%d.params", ip->model_name_prefix, qid);
	fin = fopen(filename, "r");
	if (!fin)
		return UNDEF;
    fscanf(fin, "ntopics=%d ndocs=%d nwords=%d alpha=%f beta=%f",
				&ip->K, &ip->M, &ip->V, &ip->alpha, &ip->beta);
	fclose(fin);

	snprintf(filename, sizeof(filename), "%s.%d.tassign", ip->model_name_prefix, qid);
    fin = fopen(filename, "r");
    if (!fin) {
		fprintf(stderr, "Cannot open file %d to load model!\n", filename);
		return 1;
    }
    
    textline.max_num_tokens = ip->V<<1;
	tokens = (char**) Malloc(textline.max_num_tokens, char*);
	textline.tokens = tokens;

	ip->docs = (Document*) calloc(ip->M, sizeof(Document));
    ip->z = (int**) calloc (ip->M, sizeof(int*));

    for (i = 0; i < ip->M; i++) {
		pdoc = ip->docs + i;

		line = fgets(buff, MAX_VOCAB_SIZE, fin);
		if (!line) {
	    	fprintf(stderr, "Invalid word-topic assignment file, check the number of docs!\n");
		    return UNDEF;
		}
        text_textline (buff, &textline);
		length = textline.num_tokens;	// length of this document

		pdoc->words = (int*) malloc(sizeof(int) * length);
		pdoc->cons = (long*) malloc(sizeof(long) * length);
		pdoc->length = length;
    	ip->z[i] = (int*) calloc (length, sizeof(int));
	
		for (j = 0; j < length; j++) {
			token = textline.tokens[j];
			// split the token by <word>:<topic>:<con>:<word>
			pos = strchr(token, ':');
			if (pos == NULL)
				return UNDEF;
			*pos = 0;
			word  = token;
			e.index = atoi(word);	// the word id
			pdoc->words[j] = e.index; 

			word = pos + 1;
			pos = strchr(word, ':');
			if (pos == NULL)
				return UNDEF;
			*pos = 0;
		    ip->z[i][j] = atoi(word);	// the topic

			word = pos + 1;
			pos = strchr(word, ':');
			if (pos == NULL)
				return UNDEF;
			*pos = 0;
			e.con = pdoc->cons[j] = atol(word);
			word = pos + 1;

			HASH_FIND(hh, ip->conmap, &(e.con), sizeof(long), ep);
			if (ep == NULL) {
				ep = getNextBuffEntry(ip);
				if (!ep)
					return UNDEF;
				ep->con = e.con;
				ep->index = e.index;
				snprintf(ep->word, sizeof(ep->word), "%s", word);
				HASH_ADD(hh, ip->conmap, con, sizeof(long), ep);
				HASH_ADD(hh2, ip->widmap, index, sizeof(int), ep);
			}
		}
    }

	free(tokens);
    fclose(fin);
 	return 0;	
}

static int save_model_tassign(LDAModel* ip, char* filename) {
    int i, j;
	char* token;

    FILE * fout = fopen(filename, "w");
    if (!fout) {
	    fprintf(stderr, "Cannot open file %s to save", filename);
    	return UNDEF;
    }

    // write docs with topic assignments for words
    for (i = 0; i < ip->M; i++) {
	    for (j = 0; j < ip->docs[i].length; j++) {
			if (UNDEF == ip->doc_desc.ctypes[0].con_to_token->proc (&(ip->docs[i].cons[j]), &token, contok_inst)) {
			    token = "Not in dictionary";
		        clr_err();
			}
        	///fprintf(fout, "%d:%d:%d:%s ", ip->docs[i].words[j], ip->z[i][j], ip->docs[i].cons[j], token);
        	fprintf(fout, "%d:%d:%d:%s ", ip->docs[i].words[j], topicWords[ip->docs[i].words[j]].topic, ip->docs[i].cons[j], token);
    	}
	    fprintf(fout, "\n");
    }
	free(topicWords);
    fclose(fout);

    return 0;
}

int save_model_theta(LDAModel* ip, char* filename) {
	int i, j;
    FILE * fout = fopen(filename, "w");
    if (!fout) {
	    fprintf(stderr, "Cannot open file %s to save", filename);
	    return UNDEF;
    }

    for (i = 0; i < ip->M; i++) {
	    for (j = 0; j < ip->K; j++) {
	        fprintf(fout, "%f ", ip->theta[i][j]);
    	}
	    fprintf(fout, "\n");
    }

    fclose(fout);
    return 0;
}

int save_model_phi(LDAModel* ip, char* filename) {
	int i, j;
    FILE * fout = fopen(filename, "w");
    if (!fout) {
	    fprintf(stderr, "Cannot open file %s to save", filename);
    	return UNDEF;
    }

    for (i = 0; i < ip->K; i++) {
	    for (j = 0; j < ip->V; j++) {
    	    fprintf(fout, "%f ", ip->phi[i][j]);
	    }
    	fprintf(fout, "\n");
    }

    fclose(fout);
    return 0;
}

