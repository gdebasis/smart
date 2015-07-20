/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 * C library adapted from GibbsLDA++ for LDA estimation of the
 * pseudo-relevant documents
 * init_bilda_est (SPEC *spec, int doc_fd, TR_VEC* tr_vec)
 * lda_est ()
 * close_bilda_est ()
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

#define MAX_VOCAB_SIZE 131072

// extern functions from the base class (LDAModel)....
extern void freeMatrixInt(int** p, int nr);
extern void freeMatrixDouble(float** p, int nr);
extern ConMap* getNextBuffEntry(LDAModel* ip);
extern int init_bilda_est_inst_base (BiLDAModel* ip, int doc_fd, TR_VEC* tr_vec, VEC* qvec);

//static SM_INDEX_DOCDESC src_doc_desc, tgt_doc_desc;
static int src_contok_inst, tgt_contok_inst;
static char *src_dict_file, *tgt_dict_file;
static long dict_mode, dict_mode;

float bilda_tgt_p_w_z(BiLDAModel* ip, int topicId, int wordId);

static int sampling_src(BiLDAModel* ip, int m, int n);
static int sampling_tgt(BiLDAModel* ip, int m, int n);
static int getTgtWordMap(BiLDAModel* ip, CON_WT* conwtp);
void bilda_addDoc_src(LDAModel* ip, int dict_fd, VEC *vec, int* rel_count);
void bilda_addDoc_tgt(BiLDAModel* ip, int dict_fd, VEC *vec, int* rel_count);

static void compute_phi_src(BiLDAModel* ip);
static void compute_phi_tgt(BiLDAModel* ip);
static int bilda_save_model(BiLDAModel* ip);
static int comp_topic_word (TopicWord* ptr1, TopicWord* ptr2);
static int bilda_save_model_tassign(BiLDAModel* ip, char* filename);
float p_w_z_tgt(BiLDAModel* ip, int topicId, int wordId);
float p_z_d_tgt(BiLDAModel* ip, int topicId, int docIndex);

static void compute_theta_bilda(BiLDAModel* ip);
static int bilda_save_model_phi(BiLDAModel* ip, char* filename);
static int bilda_save_model_twords(BiLDAModel* ip, char* filename);
static int contok_dict (int dict_fd, long* con, char** word);

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

void bilda_addDoc_src(LDAModel* ip, int dict_fd, VEC *vec, int* rel_count) {
	// Add document to the target side, else to the source side.
	CON_WT *conwtp;
	long   key;
	int    doclen;
	int    i, j;
	long   con;
	ConMap *e, *ep = NULL, **conmap, **widmap;
	Document* pdocs;
	char   *token;
	int*   pV;

	// Form the document object from dvec to be used by the LDA estimator
	pdocs = &ip->docs[*rel_count];
	conmap = &(ip->conmap);
	widmap = &(ip->widmap);
	pV = &(ip->V);

	// Find the length of this document
	doclen = getDocumentLength(vec);
	pdocs->words = (int*) malloc(sizeof(int) * doclen);
	pdocs->cons = (long*) malloc(sizeof(long) * doclen);
	pdocs->length = 0;

	// A hash table is maintained for counting the number of unique words
    for (i = 0, conwtp = vec->con_wtp; i < vec->ctype_len[0]; i++, conwtp++) {
		key = conwtp->con;
		HASH_FIND(hh, *conmap, &key, sizeof(long), ep);
		if (ep == NULL) {
			// a new word seen.. insert it in the hash
			e = getNextBuffEntry(ip);
			e->con = key;
			e->index = *pV;
			contok_dict(dict_fd, &key, &token);
			snprintf(e->word, sizeof(e->word), "%s", token);
			*pV = *pV + 1;
			HASH_ADD(hh, *conmap, con, sizeof(long), e);
			HASH_ADD(hh2, *widmap, index, sizeof(int), e);
			ep = e;
		}

		// A word has to be repeated tf times. Note: LDA assumes the vector
		// representation and hence word order is not important. 
		for (j = 0; j < conwtp->wt; j++) {
			pdocs->words[pdocs->length] = ep->index;  // store the word index
			pdocs->cons[pdocs->length] = ep->con; // store concept number
			pdocs->length++;
		}
   	}
	*rel_count += 1;
}

void bilda_addDoc_tgt(BiLDAModel* ip, int dict_fd, VEC *vec, int* rel_count) {
	// Add document to the target side, else to the source side.
	CON_WT *conwtp;
	long   key;
	int    doclen;
	int    i, j;
	long   con;
	ConMap *e, *ep = NULL, **conmap, **widmap;
	Document* pdocs;
	char   *token;
	int*   pV;

	// Form the document object from dvec to be used by the LDA estimator
	pdocs = &ip->docs[*rel_count];
	conmap = &(ip->conmap);
	widmap = &(ip->widmap);
	pV = &(ip->V);

	// Find the length of this document
	doclen = getDocumentLength(vec);
	pdocs->words = (int*) malloc(sizeof(int) * doclen);
	pdocs->cons = (long*) malloc(sizeof(long) * doclen);
	pdocs->length = 0;

	// A hash table is maintained for counting the number of unique words
    for (i = 0, conwtp = vec->con_wtp; i < vec->ctype_len[0]; i++, conwtp++) {
		key = conwtp->con;
		HASH_FIND(hh, *conmap, &key, sizeof(long), ep);
		if (ep == NULL) {
			// a new word seen.. insert it in the hash
			e = getNextBuffEntry(&(ip->ldaModel));
			e->con = key;
			e->index = *pV;
			contok_dict(dict_fd, &key, &token);
			snprintf(e->word, sizeof(e->word), "%s", token);
			*pV = *pV + 1;
			HASH_ADD(hh, *conmap, con, sizeof(long), e);
			HASH_ADD(hh2, *widmap, index, sizeof(int), e);
			ep = e;
		}

		// A word has to be repeated tf times. Note: LDA assumes the vector
		// representation and hence word order is not important. 
		for (j = 0; j < conwtp->wt; j++) {
			pdocs->words[pdocs->length] = ep->index;  // store the word index
			pdocs->cons[pdocs->length] = ep->con; // store concept number
			pdocs->length++;
		}
   	}
	*rel_count += 1;
}

int getNumRelDocs(TR_VEC* tr_vec) {
	int tr_ind, numRel = 0;
	for (tr_ind = 0; tr_ind < tr_vec->num_tr; tr_ind++) {
		if (tr_vec->tr[tr_ind].rel == 1)
			numRel++;
	}
	return numRel;
}

// Returns the integer id associated with the given word used in LDA estimation.
// which is the ordinal index of a concept number.
static int getTgtWordMap(BiLDAModel* ip, CON_WT* conwtp) {
	ConMap *found_conmap;
	long   key;
   
	key	= conwtp->con;
	HASH_FIND(hh, ip->conmap, &key, sizeof(long), found_conmap);
	if (!found_conmap)
		return UNDEF;
	return found_conmap->index;
}

#if 0
int con_to_wid(BiLDAModel* ip, long con) {
	ConMap *found_conmap;
	HASH_FIND(hh, ip->conmap, &con, sizeof(long), found_conmap);
	if (!found_conmap)
		return UNDEF;
	return found_conmap->index;
}

long wid_to_con(BiLDAModel* ip, int wid) {
	ConMap *found_conmap;

	// Get the concept number from the word index
	HASH_FIND(hh2, ip->widmap, &wid, sizeof(int),  found_conmap);
	if (!found_conmap)
		return UNDEF;
	return found_conmap->con;
}

int wid_to_word(BiLDAModel* ip, int wid, ConMap** conmap) {
	// Get the concept number from the word index
	HASH_FIND(hh2, ip->widmap, &wid, sizeof(int), (*conmap));
	if (!*conmap)
		return UNDEF;
	return 0;
}

int word_to_wid(BiLDAModel* ip, char* word) {
	ConMap *found_conmap;
	DICT_ENTRY dict;
	int status;

    dict.token = word;
    dict.con = UNDEF;
    ///if (UNDEF == (status = (seek_dict (dict_fd, &dict))))
	///	return (UNDEF);
    ///if (status == 0)
	///	return UNDEF;

	// Get the concept number from the word index
	HASH_FIND(hh, ip->conmap, &(dict.con), sizeof(long),  found_conmap);
	if (!found_conmap)
		return UNDEF;
	return found_conmap->index;
}
#endif

/* Return the probability of generating the given word 'w' from the given topic 'topicId' */
float bilda_tgt_p_w_z(BiLDAModel* ip, int topicId, int wordId) {
	if (!(wordId >= 0 && wordId < ip->V))
		return 0;
	if (!(topicId >= 0 && topicId < ip->ldaModel.K))
		return 0;

	return ip->phi[topicId][wordId];
}

// initialize once
int init_bilda_est(BiLDAModel* ip, SPEC* spec) {
	LDAModel *base = &ip->ldaModel;
 	SPEC_PARAM spec_args[] = {
    	{"bilda_est.num_topics", getspec_int, (char *) (&base->K)},
    	{"bilda_est.src_beta", getspec_float, (char *)&(base->beta)},
    	{"bilda_est.tgt_beta", getspec_float, (char *) (&ip->beta)},
	    {"bilda_est.iters", getspec_int, (char *) &(base->niters)},
    	{"bilda_est.model_name_prefix", getspec_string, (char *) &(base->model_name_prefix)},
	    {"bilda_est.twords", getspec_int, (char *) &(base->twords)},
    	{"bilda_est.tosave", getspec_bool, (char *) &(base->tosave)},
	   	{"bilda_est.src.dict_file",  getspec_dbfile, (char *) &src_dict_file},
    	{"bilda_est.src.dict_file.rmode",  getspec_filemode, (char *) &dict_mode},
	   	{"bilda_est.tgt.dict_file",  getspec_dbfile, (char *) &tgt_dict_file},
    	{"bilda_est.tgt.dict_file.rmode",  getspec_filemode, (char *) &dict_mode}
	};
	int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

    PRINT_TRACE (2, print_string, "Trace: entering init_bilda_est");

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return UNDEF;
    if (UNDEF == (ip->src_dict_fd = open_dict (src_dict_file, dict_mode)))
		return UNDEF;
    if (UNDEF == (ip->tgt_dict_fd = open_dict (tgt_dict_file, dict_mode)))
		return UNDEF;

	ip->ldaModel.buffSize = MAX_VOCAB_SIZE;
	ip->ldaModel.buff = (ConMap*) malloc (sizeof(ConMap) * ip->ldaModel.buffSize);
	ip->ldaModel.conmap_buffptr = ip->ldaModel.buff;

    PRINT_TRACE (2, print_string, "Trace: leaving init_bilda_est");
}

int init_bilda_est_inst (BiLDAModel* ip, int doc_fd, int tgt_doc_fd, TR_VEC* tr_vec, TR_VEC* tgt_tr_vec, VEC* qvec) {
	int m, n, w, k, tr_ind;
	int N, topic, rel_count = 0;
	VEC dvec;
	LDAModel *base = &(ip->ldaModel);

    PRINT_TRACE (2, print_string, "Trace: entering init_bilda_est_inst");

	init_bilda_est_inst_base(ip, doc_fd, tr_vec, qvec);

	ip->V = 0;		// initialize the vocab size to 0 for this set of documents
	ip->doc_fd = tgt_doc_fd;	// document file of the target language
	ip->tr_vec = tgt_tr_vec;
	ip->conmap = NULL;
	ip->widmap = NULL;

	ip->M = getNumRelDocs(tgt_tr_vec);
	ip->min_M = MIN(ip->ldaModel.M, ip->M);
	if (ip->min_M == 0)
		return 0;

	ip->docs = (Document*) malloc(sizeof(Document) * ip->min_M);
	ip->p = (float*) Malloc(ip->ldaModel.K, float);

	// Iterate for every document in the target language and add these
	// to the array... the source documents have already been added during the base class instantiation
	for (tr_ind = 0; tr_ind < ip->tr_vec->num_tr; tr_ind++) {
		if (rel_count >= ip->min_M)
			break;
		if (ip->tr_vec->tr[tr_ind].rel == 0)
			continue;		// skip non-relevant documents

		/* Open the document vector */
	    dvec.id_num.id = ip->tr_vec->tr[tr_ind].did;
	    if (UNDEF == seek_vector (ip->doc_fd, &dvec) ||
	        UNDEF == read_vector (ip->doc_fd, &dvec)) {
    	    return UNDEF;
	    }
		// Form the document object from dvec to be used by the LDA estimator
		bilda_addDoc_tgt(ip, ip->tgt_dict_fd, &dvec, &rel_count);
    }

    // K: from command line or default value
    // alpha, beta: from command line or default values
    // niters, savestep: from command line or default values
	// nw is a VxK matrix
    ip->nw = (int**) calloc (ip->V, sizeof(int*));
    for (w = 0; w < ip->V; w++) {
        ip->nw[w] = (int*) calloc (ip->ldaModel.K, sizeof(int));
    }
	
    ip->nd = (int**) calloc (ip->min_M, sizeof(int*));
    for (m = 0; m < ip->min_M; m++) {
        ip->nd[m] = (int*) calloc (ip->ldaModel.K, sizeof(int));
    }
	
    ip->nwsum = (int*) calloc (ip->ldaModel.K, sizeof(int));
    ip->ndsum = (int*) calloc (ip->min_M, sizeof(int));;

    srandom(time(0)); // initialize for random number generation
    ip->z = (int**) calloc (ip->min_M, sizeof(int*));

    for (m = 0; m < ip->min_M; m++) {
		N = ip->docs[m].length;
    	ip->z[m] = (int*) calloc (N, sizeof(int));;
	
        // initialize for z
        for (n = 0; n < N; n++) {
    	    topic = (int)(((float)random() / RAND_MAX) * ip->ldaModel.K);
    	    ip->z[m][n] = topic;
    	  
		   	if (!(topic < ip->ldaModel.K && ip->docs[m].words[n] < ip->V))
				return UNDEF;
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
    
    ip->phi = (float**) calloc (ip->ldaModel.K, sizeof(float*));
    for (k = 0; k < ip->ldaModel.K; k++) {
        ip->phi[k] = (float*) calloc(ip->V, sizeof(float));
    }    
    
    PRINT_TRACE (2, print_string, "Trace: leaving init_bilda_est_inst");
    return 0;
}

int close_bilda_est (BiLDAModel* ip) {
    PRINT_TRACE (2, print_string, "Trace: entering close_bilda_est");

	if (UNDEF == close_dict(ip->src_dict_fd))
		return UNDEF;
	if (UNDEF == close_dict(ip->tgt_dict_fd))
		return UNDEF;

	free(ip->ldaModel.buff);

    PRINT_TRACE (2, print_string, "Trace: leaving close_bilda_est");
}

int close_bilda_est_inst (BiLDAModel* ip)
{
	int i;
	Document *pdoc;

    PRINT_TRACE (2, print_string, "Trace: entering close_bilda_est_inst");

	close_lda_est_inst(&(ip->ldaModel));	// Free the LDA resources.

	if (ip->min_M == 0)
		return;

	freeMatrixInt(ip->nw, ip->V);
	freeMatrixInt(ip->nd, ip->min_M);
	free(ip->nwsum);
	free(ip->ndsum);
	free(ip->p);

	freeMatrixInt(ip->z, ip->min_M);
	freeMatrixDouble(ip->phi, ip->ldaModel.K);

	for (i = 0; i < ip->min_M; i++) {
		pdoc = ip->docs + i;
		free(pdoc->words);
		free(pdoc->cons);
	}
	free(ip->docs);

	HASH_CLEAR(hh, ip->conmap);
	HASH_CLEAR(hh2, ip->widmap);

    PRINT_TRACE (2, print_string, "Trace: leaving close_bilda_est_inst");
    return 0;
}

int bilda_est(BiLDAModel* ip) {
	static char msg[256];
	int iter;
	int m, n, topic;

	if (ip->min_M == 0)
		return 0;

    PRINT_TRACE (2, print_string, "Trace: entering lda_est");
    snprintf(msg, sizeof(msg), "Sampling %d iterations!\n", ip->ldaModel.niters);
	PRINT_TRACE(4, print_string, msg);

    for (iter = 0; iter <= ip->ldaModel.niters; iter++) {
		// for all z_i
		for (m = 0; m < ip->min_M; m++) {
			// Generate a source document
	    	for (n = 0; n < ip->ldaModel.docs[m].length; n++) {
				// (z_i = z[m][n])
				// sample from p(z_i|z_-i, w)
				topic = sampling_src(ip, m, n);
				ip->ldaModel.z[m][n] = topic;
	    	}

			// Generate a target document
	    	for (n = 0; n < ip->docs[m].length; n++) {
				// (z_i = z[m][n])
				// sample from p(z_i|z_-i, w)
				topic = sampling_tgt(ip, m, n);
				ip->z[m][n] = topic;
    		}
		}
    }
    
    snprintf(msg, sizeof(msg), "Gibbs sampling completed for topic %d", ip->tr_vec->qid);
	PRINT_TRACE(4, print_string, msg);
    snprintf(msg, sizeof(msg), "Saving the final model for topic %d", ip->tr_vec->qid);
	PRINT_TRACE(4, print_string, msg);

    compute_theta_bilda(ip);
    compute_phi_src(ip);	// compute phi_s
	compute_phi_tgt(ip);	// compute phi_t

	// Save the model for future use.
	if (ip->ldaModel.tosave) {
	    if (bilda_save_model(ip) == UNDEF)
			return UNDEF;
	}

	return 0;
}

static void compute_theta_bilda(BiLDAModel* ip) {
	int m, k;

    for (m = 0; m < ip->min_M; m++) {
		for (k = 0; k < ip->ldaModel.K; k++) {
	    	ip->ldaModel.theta[m][k] = (ip->ldaModel.nd[m][k] + ip->nd[m][k] + ip->ldaModel.alpha) /
										(ip->ldaModel.ndsum[m] + ip->ndsum[m] + ip->ldaModel.K*ip->ldaModel.alpha);
		}
    }
}

static int sampling_src(BiLDAModel* ip, int m, int n) {
    // remove z_i from the count variables
	LDAModel *base = &(ip->ldaModel);
    int topic = base->z[m][n];
    int w = base->docs[m].words[n];
	int k;
	float u;

    base->nw[w][topic] -= 1;
    base->nd[m][topic] -= 1;
    base->nwsum[topic] -= 1;
    base->ndsum[m] -= 1;

    float Vbeta = base->V * base->beta;
    float Kalpha = base->K * base->alpha;

    // do multinomial sampling via cumulative method
    for (k = 0; k < base->K; k++) {
		base->p[k] = (base->nw[w][k] + base->beta) / (base->nwsum[k] + Vbeta) *
				(base->nd[m][k] + ip->nd[m][k] + base->alpha) / (base->ndsum[m] + ip->ndsum[m] + 1 + Kalpha);
    }
    
	// cumulate multinomial parameters
    for (k = 1; k < base->K; k++) {
		base->p[k] += base->p[k - 1];
    }

    // scaled sample because of unnormalized p[]
    u = ((float)random() / RAND_MAX) * base->p[base->K - 1];
    
    for (topic = 0; topic < base->K-1; topic++) {
		if (base->p[topic] > u) {
	    	break;
		}
    }
    
    // add newly estimated z_i to count variables
    base->nw[w][topic] += 1;
    base->nd[m][topic] += 1;
    base->nwsum[topic] += 1;
    base->ndsum[m] += 1;    
    
    return topic;
}

static int sampling_tgt(BiLDAModel* ip, int m, int n) {
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
    float Kalpha = ip->ldaModel.K * ip->ldaModel.alpha;

    // do multinomial sampling via cumulative method
    for (k = 0; k < ip->ldaModel.K; k++) {
		ip->p[k] = (ip->nw[w][k] + ip->beta) / (ip->nwsum[k] + Vbeta) *
		    (ip->nd[m][k] + ip->ldaModel.nd[m][k] + ip->ldaModel.alpha) / (ip->ndsum[m] + ip->ldaModel.ndsum[m] + 1 + Kalpha);
    }
    
	// cumulate multinomial parameters
    for (k = 1; k < ip->ldaModel.K; k++) {
		ip->p[k] += ip->p[k - 1];
    }

    // scaled sample because of unnormalized p[]
    u = ((float)random() / RAND_MAX) * ip->p[ip->ldaModel.K - 1];
    
    for (topic = 0; topic < ip->ldaModel.K-1; topic++) {
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

static void compute_phi_src(BiLDAModel* ip) {
    int k, w;
	LDAModel *base = &(ip->ldaModel);
    for (k = 0; k < base->K; k++) {
		for (w = 0; w < base->V; w++) {
			base->phi[k][w] = (base->nw[w][k] + base->beta) / (base->nwsum[k] + base->V * base->beta);
		}
    }
}

static void compute_phi_tgt(BiLDAModel* ip) {
	int k, w;
    for (k = 0; k < ip->ldaModel.K; k++) {
		for (w = 0; w < ip->V; w++) {
		    ip->phi[k][w] = (ip->nw[w][k] + ip->beta) / (ip->nwsum[k] + ip->V * ip->beta);
		}
    }
}

int bilda_save_model(BiLDAModel* ip) {
	static char filename[256];
	int qid = ip->ldaModel.qvec->id_num.id;
	LDAModel *base = &(ip->ldaModel);

	snprintf(filename, sizeof(filename), "%s.%d.tassign", base->model_name_prefix, qid); 
    if (bilda_save_model_tassign(ip, filename) == UNDEF) {
	    return UNDEF;
    }

	if (sm_trace >= 4) {
		snprintf(filename, sizeof(filename), "%s.%d.phi", base->model_name_prefix, qid); 
	    if (bilda_save_model_phi(ip, filename) == UNDEF) {
    		return UNDEF;
	    }
		// There is only one theta to write... call the base function here...
		snprintf(filename, sizeof(filename), "%s.%d.theta", base->model_name_prefix, qid); 
    	if (save_model_theta(ip, filename) == UNDEF) {
	    	return UNDEF;
	    }
	}

	if (ip->ldaModel.twords > 0) {
		snprintf(filename, sizeof(filename), "%s.%d.twords", base->model_name_prefix, qid);
		if (UNDEF == bilda_save_model_twords(ip, filename))
			return UNDEF;
	}
    return 0;
}

static int bilda_save_model_phi(BiLDAModel* ip, char* filename) {
	int i, j;
    FILE * fout;
	static char full_filename[256];

	snprintf(full_filename, sizeof(full_filename), "src_%s", filename);
	save_model_phi(ip->ldaModel, full_filename);

	snprintf(full_filename, sizeof(full_filename), "tgt_%s", filename);
	fout = fopen(full_filename, "w");
    if (!fout) {
	    fprintf(stderr, "Cannot open file %s to save", full_filename);
    	return UNDEF;
    }

    for (i = 0; i < ip->ldaModel.K; i++) {
	    for (j = 0; j < ip->V; j++) {
    	    fprintf(fout, "%f ", ip->phi[i][j]);
	    }
    	fprintf(fout, "\n");
    }

    fclose(fout);
    return 0;
}

static int comp_topic_word (TopicWord* ptr1, TopicWord* ptr2) {
	return ptr1->phi < ptr2->phi? -1 : ptr1->phi == ptr2->phi? 0 : 1;
}

static int comp_topic_word_descending (TopicWord* ptr1, TopicWord* ptr2) {
	return -1 * comp_topic_word(ptr1, ptr2);
}

static int contok_dict (int dict_fd, long* con, char** word)
{
    DICT_ENTRY dict;
    int status;

    dict.con = *con;
    dict.info = 0;
    dict.token = (char *) NULL;
    if (UNDEF == (status = seek_dict(dict_fd, &dict))) {
        return (UNDEF);
    }
    if (status == 0) {
        *word = "Not in Dictionary";
    }
    else {
        if (1 != (status = read_dict (dict_fd, &dict)))
           *word = "Not in Dictionary";
        else
           *word = dict.token;
    }
    return (status);
}

// Print out the top N most likely words of a topic
static int bilda_save_model_twords(BiLDAModel* ip, char* filename) {
	int k, w, key;
	ConMap *found_conmap;
	TopicWord *topicWords, *twp;
	char *token;
	int  twords;
	LDAModel* base = &(ip->ldaModel);
	FILE* fout;
	int   maxV;
	static char full_filename[256];

	// Print the source topic words
	snprintf(full_filename, sizeof(full_filename), "src_%s", filename);
    fout = fopen(full_filename, "w");
    if (!fout) {
		fprintf(stderr, "Cannot open file %s to save!\n", full_filename);
		return UNDEF;
    }

	maxV = MAX(base->V, ip->V);
	base->twords = MIN(base->twords, base->V);
	topicWords = (TopicWord*) malloc (sizeof(TopicWord) * maxV);

    for (k = 0; k < base->K; k++) {
		twp = topicWords;
	    for (w = 0; w < base->V; w++) {
			key	= w;
			// Get the concept number from the word index
			HASH_FIND(hh2, base->widmap, &key, sizeof(int),  found_conmap);
			if (!found_conmap)
				return UNDEF;
			twp->con = found_conmap->con;
			twp->w = w;
			twp->phi = base->phi[k][w];
			twp++;
	    }
		// sort in ascending order of phi
		qsort(topicWords, twp - topicWords, sizeof(TopicWord), comp_topic_word);
		fprintf(fout, "Topic %d:\n", k);
		// print the top 'twords' phis
		for (twords = 0, twp = twp-1; twp >= topicWords && twords < base->twords; twp--, twords++) {
			// Get the word string from the concept number
			contok_dict(ip->src_dict_fd, &(twp->con), &token);
			fprintf(fout, "%d %d %s %f\n", twp->w, twp->con, token, twp->phi);
		}
	}
	fclose(fout);

	// Print the target topic words
	snprintf(full_filename, sizeof(full_filename), "tgt_%s", filename);
    fout = fopen(full_filename, "w");
    if (!fout) {
		fprintf(stderr, "Cannot open file %s to save!\n", full_filename);
		return UNDEF;
    }

    for (k = 0; k < base->K; k++) {
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
		for (twords = 0, twp = twp-1; twp >= topicWords && twords < base->twords; twp--, twords++) {
			// Get the word string from the concept number
			contok_dict(ip->tgt_dict_fd, &(twp->con), &token);
			fprintf(fout, "%d %d %s %f\n", twp->w, twp->con, token, twp->phi);
		}
	}
	fclose(fout);

	free(topicWords);
    return 0;
}

static int bilda_save_model_tassign(BiLDAModel* ip, char* filename) {
    int i, j;
	char* token;
	static char full_filename[256];
	LDAModel* base = &(ip->ldaModel);
    FILE * fout;
  
	snprintf(full_filename, sizeof(full_filename), "src_%s", filename);    	
	fout = fopen(full_filename, "w");
    if (!fout) {
	    fprintf(stderr, "Cannot open file %s to save", full_filename);
    	return UNDEF;
    }

    // write docs with topic assignments for words for the source side
    for (i = 0; i < ip->min_M; i++) {
	    for (j = 0; j < base->docs[i].length; j++) {
			contok_dict(ip->src_dict_fd, &(base->docs[i].cons[j]), &token);
        	fprintf(fout, "%d:%d:%d:%s ", base->docs[i].words[j], base->z[i][j], base->docs[i].cons[j], token);
    	}
	    fprintf(fout, "\n");
    }
	fclose(fout);

    // write docs with topic assignments for words for the target side
	snprintf(full_filename, sizeof(full_filename), "tgt_%s", filename);    	
	fout = fopen(full_filename, "w");
    if (!fout) {
	    fprintf(stderr, "Cannot open file %s to save", full_filename);
    	return UNDEF;
    }
    for (i = 0; i < ip->min_M; i++) {
	    for (j = 0; j < ip->docs[i].length; j++) {
			contok_dict(ip->tgt_dict_fd, &(ip->docs[i].cons[j]), &token);
        	fprintf(fout, "%d:%d:%d:%s ", ip->docs[i].words[j], ip->z[i][j], ip->docs[i].cons[j], token);
    	}
	    fprintf(fout, "\n");
    }

    fclose(fout);
    return 0;
}

float bilda_compute_marginalized_P_w_D_src(BiLDAModel* ip, CON_WT* conwtp, int docIndex) {
	float lda = 0;
	int   i, wordId;

	wordId = getWordMap(&(ip->ldaModel), conwtp);
	for (i = 0; i < ip->ldaModel.K; i++) {
		lda += p_w_z(&(ip->ldaModel), i, wordId) * p_z_d(&(ip->ldaModel), i, docIndex);
	}
	return lda;
}

float bilda_compute_marginalized_P_w_D_tgt(BiLDAModel* ip, CON_WT* conwtp, int docIndex) {
	float lda = 0;
	int   i, wordId;

	wordId = getTgtWordMap(ip, conwtp);
	for (i = 0; i < ip->ldaModel.K; i++) {
		lda += p_w_z_tgt(ip, i, wordId) * p_z_d_tgt(ip, i, docIndex);
	}
	return lda;
}

/* Return the probability of generating the given word 'w' from the given topic 'topicId' */
float p_w_z_tgt(BiLDAModel* ip, int topicId, int wordId) {
	if (!(wordId >= 0 && wordId < ip->V))
		return 0;
	if (!(topicId >= 0 && topicId < ip->ldaModel.K))
		return 0;

	return ip->phi[topicId][wordId];
}

/* Return the probability of generating the given word topic from the given document */
float p_z_d_tgt(BiLDAModel* ip, int topicId, int docIndex) {
	if (!(docIndex >= 0 && docIndex < ip->min_M))
		return 0;
	if (!(topicId >= 0 && topicId < ip->ldaModel.K))
		return 0;

	return ip->ldaModel.theta[docIndex][topicId];
}


