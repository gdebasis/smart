/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 * A modification of LDA for the infinite mixture model case, i.e.
 * a non-parametric version of the model.
 * TRLM with a non-parametric LDA does not need a predefined
 * number of topics and hence the number of topics can automatically
 * be adapted according to the specificity of a query.
 * init_ilda_est (SPEC *spec, int doc_fd, TR_VEC* tr_vec)
 * ilda_est ()
 * close_ilda_est ()
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
#define MAX_TOPICS 1000

static int contok_inst;
static char *dict_file;
static long dict_mode;
char* basepath;

static TopicWord* topicWords;

static int sampling(LDAModel* ip, int m, int n);

// initialize once
int init_ilda_est(LDAModel* ip, SPEC* spec) {
	SPEC_PARAM spec_args[] = {
    	{"lda_est.dict_file",  getspec_dbfile, (char *) &dict_file},
    	{"lda_est.dict_file.rmode",  getspec_filemode, (char *) &dict_mode},
	    {"lda_est.alpha", getspec_float, (char *) &ip->alpha},
    	{"lda_est.beta", getspec_float, (char *) &ip->beta},
	    {"lda_est.iters", getspec_int, (char *) &ip->niters},
    	{"lda_est.model_name_prefix", getspec_string, (char *) &ip->model_name_prefix},
    	{"ilda.basepath", getspec_string, (char *) &basepath},
	    {"lda_est.twords", getspec_int, (char *) &ip->twords},
    	{"lda_est.tosave", getspec_bool, (char *) &ip->tosave},
    	{"lda_est.rel_only", getspec_bool, (char *) &ip->rel_only},
    	//{"lda_est.vocab_size", getspec_int, (char *) &ip->buffSize},
    	TRACE_PARAM ("lda_est.trace")
	}; 
	int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

    PRINT_TRACE (2, print_string, "Trace: entering init_ilda_est");

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

    PRINT_TRACE (2, print_string, "Trace: leaving init_ilda_est");
}

int init_ilda_est_inst (LDAModel* ip, int doc_fd, TR_VEC* tr_vec, VEC* qvec, int multifaceted) {
	int m, n, w, k;
	int N, topic;
	int status;

    PRINT_TRACE (2, print_string, "Trace: entering init_ilda_est_inst");

	ip->conmap_buffptr = ip->buff;	// reset buffer
	ip->doc_fd = doc_fd;
	ip->tr_vec = tr_vec;
	ip->qvec = qvec;
	ip->multifaceted = multifaceted;
	ip->conmap = NULL;
	ip->widmap = NULL;
	ip->K = 1;

	ip->M = getNumRelDocs(tr_vec);
	if (ip->M == 0)
		return UNDEF;

#ifdef __ILDA_COMPUTATION
	ip->p = (float*) Malloc(MAX_TOPICS, float);
#endif
	status = constructDataset(ip);
	if (status == UNDEF)
		return UNDEF;

#ifdef __ILDA_COMPUTATION
    // K: from command line or default value
    // alpha, beta: from command line or default values
    // niters, savestep: from command line or default values
	// nw is a VxK matrix
    ip->nw = (int**) calloc (ip->V, sizeof(int*));
    for (w = 0; w < ip->V; w++) {
        ip->nw[w] = (int*) calloc (MAX_TOPICS, sizeof(int));
    }
	
    ip->nd = (int**) calloc (ip->M, sizeof(int*));
    for (m = 0; m < ip->M; m++) {
        ip->nd[m] = (int*) calloc (MAX_TOPICS, sizeof(int));
    }
	
    ip->nwsum = (int*) calloc (MAX_TOPICS, sizeof(int));
    ip->ndsum = (int*) calloc (ip->M, sizeof(int));;

    srandom(time(0)); // initialize for random number generation
    ip->z = (int**) calloc (ip->M, sizeof(int*));

    for (m = 0; m < ip->M; m++) {
		N = ip->docs[m].length;
    	ip->z[m] = (int*) calloc (N, sizeof(int));;
	
        // initialize for z
        for (n = 0; n < N; n++) {
    	    topic = 0;  // only one topic at start
    	    ip->z[m][n] = topic;
    	  
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
        ip->theta[m] = (float*) calloc(MAX_TOPICS, sizeof(float));
    }
	
    ip->phi = (float**) calloc (MAX_TOPICS, sizeof(float*));
    for (k = 0; k < MAX_TOPICS; k++) {
        ip->phi[k] = (float*) calloc(ip->V, sizeof(float));
    }    
#endif
    
    PRINT_TRACE (2, print_string, "Trace: leaving init_ilda_est_inst");
    return 0;
}

int ilda_est(LDAModel* ip) {

	static char msg[256];
	int m, n, topic;

#ifdef __ILDA_COMPUTATION		
	int iter;

    PRINT_TRACE (2, print_string, "Trace: entering ilda_est");

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

    compute_theta(ip);
    compute_phi(ip);
	return 0;
#endif

	// I'm not going to take the trouble of implementing the
	// Non-parametric LDA. Simply call the Java tool by Heinrich.
	// Write out SVM formatted file on which his Java code can work.
	// Read back the files generated by his code to populate
	// the matrices theta and phi.
	// Write out the corpus (comprised of the num_rel docs)
	// on which the non-parametric LDA will be inferred

	writeTopDocs(ip);

	snprintf(msg, sizeof(msg), "/mnt/sdb2/smart/src/run_ilda.sh %d", ip->tr_vec->qid); 	
	system(msg);

	load_matrices(ip);
   
	return 0;
}

void writeTopDocs(LDAModel* ip) {

	int tr_ind, m = -1, i, wordid;
	static char filename[256];
	FILE* fout;

	snprintf(filename, sizeof(filename), "%s/corpus.%d.txt", basepath, ip->tr_vec->qid);
	fout = fopen(filename, "w"); 
	CON_WT *conwtp;
	VEC dvec;

	for (tr_ind = 0; tr_ind < ip->tr_vec->num_tr; tr_ind++) {
		if (ip->rel_only) {
			if (ip->tr_vec->tr[tr_ind].rel == 0)
				continue;		// skip non-relevant documents
			m++;
		}
		else
			m++;

		/* Open the document vector */
	    dvec.id_num.id = ip->tr_vec->tr[tr_ind].did;
	    if (UNDEF == seek_vector (ip->doc_fd, &dvec) ||
	        UNDEF == read_vector (ip->doc_fd, &dvec)) {
    	    return UNDEF;
	    }

		fprintf(fout, "%d ", dvec.ctype_len[0]);   
    	for (i = 0, conwtp = dvec.con_wtp; i < dvec.ctype_len[0]; i++, conwtp++) {
			wordid = con_to_wid(ip, conwtp->con);
			fprintf(fout, "%d:%d ", wordid, (int)conwtp->wt); 
		}
		fprintf(fout, "\n");
	}	

	fclose(fout);
}

// Load theta and phi matrices
void load_matrices(LDAModel *ip) {
	static char filename[256];
	char** tokens, *token;
	SM_TEXTLINE textline;
	int m, k, j, qid, length;
	float value;
	FILE *fin;
	static char buff[262144]; // 2^18

	qid = ip->qvec->id_num.id;

	// Read parameters from params file
	snprintf(filename, sizeof(filename), "%s/%d.params", basepath, qid);
	fin = fopen(filename, "r");
	if (!fin)
		return UNDEF;
    fscanf(fin, "%d %d %d", &ip->K, &ip->M, &ip->V);
	fclose(fin);

	// Init theta and phi...
    ip->theta = (float**) calloc (ip->M, sizeof(float*));
    for (m = 0; m < ip->M; m++) {
        ip->theta[m] = (float*) calloc(ip->K, sizeof(float));
    }
	
    ip->phi = (float**) calloc (ip->K, sizeof(float*));
    for (k = 0; k < ip->K; k++) {
        ip->phi[k] = (float*) calloc(ip->V, sizeof(float));
    }

	// Load phi matrix
	snprintf(filename, sizeof(filename), "%s/%d.ilda.phi", basepath, qid);
    fin = fopen(filename, "r");
    if (!fin) {
		fprintf(stderr, "Cannot open phi file %s\n", filename);
		return 1;
    }

    textline.max_num_tokens = ip->V;
	tokens = (char**) Malloc(textline.max_num_tokens, char*);
	textline.tokens = tokens;

	k = 0;
	while (NULL != fgets(buff, sizeof(buff), fin)) {
        text_textline (buff, &textline);
		length = textline.num_tokens;	// length of this document
		assert(length == ip->V);

		for (j = 0; j < length; j++) {
			token = textline.tokens[j];
			sscanf(token, "%f", &value);
			ip->phi[k][j] = value; 
		}
		k++;
	}
	assert(k == ip->K);
	fclose(fin);

	// Load theta matrix
	snprintf(filename, sizeof(filename), "%s/%d.ilda.theta", basepath, qid);
    fin = fopen(filename, "r");
    if (!fin) {
		fprintf(stderr, "Cannot open phi file %s\n", filename);
		return 1;
    }
	free(tokens);

    textline.max_num_tokens = ip->K;
	tokens = (char**) Malloc(textline.max_num_tokens, char*);
	textline.tokens = tokens;
	m = 0;
	while (NULL != fgets(buff, sizeof(buff), fin)) {
        text_textline (buff, &textline);
		length = textline.num_tokens;	// length of this document
		assert(length == ip->K);

		for (j = 0; j < length; j++) {
			token = textline.tokens[j];
			sscanf(token, "%f", &value);
			ip->theta[m][j] = value; 
		}
		m++;
	}
	assert(m == ip->M);
	fclose(fin);

	free(tokens);
}

int close_ilda_est_inst (LDAModel* ip)
{
	int i;
	Document *pdoc;

    PRINT_TRACE (2, print_string, "Trace: entering close_ilda_est_inst");

#ifdef __ILDA_COMPUTATION	
	freeMatrixInt(ip->z, ip->M);
	freeMatrixDouble(ip->phi, MAX_TOPICS);
	freeMatrixInt(ip->nw, ip->V);
	freeMatrixInt(ip->nd, ip->M);
	free(ip->nwsum);
	free(ip->ndsum);
	free(ip->p);
#endif

	freeMatrixDouble(ip->theta, ip->M);

	for (i = 0; i < ip->M; i++) {
		pdoc = ip->docs + i;
		free(pdoc->words);
		free(pdoc->cons);
	}
	free(ip->docs);

	HASH_CLEAR(hh, ip->conmap);
	HASH_CLEAR(hh2, ip->widmap);

    PRINT_TRACE (2, print_string, "Trace: leaving close_ilda_est_inst");
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
    // do multinomial sampling via cumulative method
    for (k = 0; k < ip->K; k++) {
		ip->p[k] = (ip->nw[w][k] + ip->beta) / (ip->nwsum[k] + Vbeta) *
		    (ip->nd[m][k] + ip->alpha) / (ip->ndsum[m] + ip->K * ip->alpha);
    }
	// sampling prob. for new topic
	ip->p[ip->K] = 1/(float)ip->V *  ip->alpha / (ip->ndsum[m] + ip->K * ip->alpha);
    
	// cumulate multinomial parameters
    for (k = 1; k < ip->K+1; k++) {
		ip->p[k] += ip->p[k - 1];
    }

    // scaled sample because of unnormalized p[]
    u = ((float)random() / (float)RAND_MAX) * ip->p[ip->K];
    
    for (topic = 0; topic < ip->K; topic++) {
		if (ip->p[topic] > u) {
	    	break;
		}
    }

	if (topic == ip->K) {
		// we have to create a new topic now
		if (ip->K < MAX_TOPICS-1) {
			ip->K++;
			printf("Creating a new topic. Total topics: %d\n", ip->K);
		}
	}
	
    
    // add newly estimated z_i to count variables
    ip->nw[w][topic] += 1;
    ip->nd[m][topic] += 1;
    ip->nwsum[topic] += 1;
    ip->ndsum[m] += 1;    
    
    return topic;
}


