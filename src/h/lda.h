#ifndef LDA_H
#define LDA_H

#include "uthash.h"
#include "docdesc.h"

typedef struct Document {
	int*   words;
	long*  cons;
	int    length;
} Document;

typedef struct {
	long  con;
	int   index;
	char  word[32];
    UT_hash_handle hh;         /* key by con */
    UT_hash_handle hh2;        /* alternative key by index */
} ConMap;

// LDA parameters
typedef struct {
	char*     model_name_prefix;   // model name prefix

	int       K;   // number of topics
	int       twords;
	int       M;   // number of documents
	int       V;   // number of unique words in a set of documents
	float     alpha;	// hyperparameter of theta 
	float     beta;  // hyperparameter of phi

	VEC*      qvec;
	TR_VEC*   tr_vec;	// pseudo relevant documents
	int       doc_fd;	// doc vector file fd

	Document* docs;

	float**  theta;	    // M x K : generation of z from d	
	float**  phi;		// K x V : generation of w from z
	int**     z;		// topic assignments for words, size M x doc.size()
	int       niters;	// number of iterations for LDA

	int   **nw; // cwt[i][j]: number of instances of word/term i assigned to topic j, size V x K	
    int   **nd; // na[i][j]: number of words in document i assigned to topic j, size M x K
	int   *nwsum; // nwsum[j]: total number of words assigned to topic j, size K
    int   *ndsum; // nasum[i]: total number of words in document i, size M
    float *p;

	int     tosave;  // whether to save this model
	int     multifaceted;	// whether an unifaceted or multifaceted
	ConMap* conmap;	// hash table for storing the word indexes keyed by cons
	ConMap* widmap;	// hash table for storing the conwts keyed by word index

	// Buffer of ConMaps
	ConMap* buff;
	int     buffSize;
	ConMap* conmap_buffptr;

	SM_INDEX_DOCDESC doc_desc;
	int rel_only;
} LDAModel;

// Contains additional parameters for a second language
typedef struct {
	LDAModel  ldaModel;		// pointer to the base object

	int       M;
	int       min_M;
	int       src_dict_fd;
	int       tgt_dict_fd;
	float     beta;
	int**     z;		// topic assignments for words, size M x doc.size()
	int       V;			// vocabulary size of the tgt language	
	int       doc_fd;		// doc vector file fd on the target side
	float**   phi;			// modelling topic-word distributions in the target language
	Document* docs;			// target language documents each paired with a document of the source side
	TR_VEC*   tr_vec;	// pseudo relevant documents on the target side
	ConMap*   conmap;		// hash table for storing the word indexes keyed by cons (for tgt language)
	ConMap*   widmap;		// hash table for storing the conwts keyed by word index (for tgt language)
	int**	  nw; 			// cwt[i][j]: number of instances of word/term i assigned to topic j, size V x K	
    int**	  nd; 			// na[i][j]: number of words in document i assigned to topic j, size M x K
	int*	  nwsum; 			// nwsum[j]: total number of words assigned to topic j, size K
    int*	  ndsum; 			// nasum[i]: total number of words in document i, size M
    float*    p;			// sampled z values for the target language
}
BiLDAModel;

typedef struct {
	float phi;
	int   w;
	long  con;
	int   topic;	// assigned topic with max. prob.
} TopicWord;

extern int init_lda_est(LDAModel* ip, SPEC* spec);
extern int init_lda_est_inst (LDAModel* ip, int doc_fd, TR_VEC* tr_vec, VEC* qvec, int multifaceted);
extern int init_lda_est_ref_inst (LDAModel* ip, char* saved_model_name_prefix, VEC* qvec);
extern int lda_est(LDAModel* ip);
extern int init_lda_est_inst_coll (LDAModel* ip, int doc_fd, int M);
extern int close_lda_est (LDAModel* ip);
extern int close_lda_est_inst (LDAModel* ip);

// ILDA
extern int init_ilda_est(LDAModel* ip, SPEC* spec);
extern int init_ilda_est_inst (LDAModel* ip, int doc_fd, TR_VEC* tr_vec, VEC* qvec, int multifaceted);
extern int ilda_est(LDAModel* ip);
extern int close_ilda_est_inst (LDAModel* ip);

// Auxilliary functions...
extern double compute_posterior(LDAModel* ip);
extern float compute_marginalized_P_w_D(LDAModel* ldamodel, CON_WT* conwtp, int docIndex);
extern float compute_single_topic_P_w_D(LDAModel* ip, CON_WT* conwtp, int docIndex, int topicIndex);
extern float per_topic_p_w_d(LDAModel* ip, CON_WT* conwtp, int docIndex);
extern float getMarginalizedQueryGenEstimate(LDAModel*, int, float*, long);
extern float p_w_z(LDAModel* ip, int topicId, int wordId);
extern float p_z_d(LDAModel* ip, int topicId, int docIndex);
extern double compute_posterior(LDAModel* ip);
extern int init_lda_est_ref (LDAModel* ip, char* saved_model_name_prefix, VEC* qvec);
extern int con_to_wid(LDAModel* ip, long con);
extern long wid_to_con(LDAModel* ip, int wid);
extern int wid_to_word(LDAModel* ip, int wid, ConMap** conmap);
extern int word_to_wid(LDAModel* ip, char* word);
extern float getProbWordFromTopic(LDAModel* model, CON_WT* conwtp, int k);
extern float p_con_z(LDAModel* ip, int topicId, long con);

extern void compute_theta(LDAModel* ip);
extern void compute_phi(LDAModel* ip);
#endif
