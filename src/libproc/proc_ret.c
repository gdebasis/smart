#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libproc/proc_ret.c,v 11.2 1993/02/03 16:53:44 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "proc.h"

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table giving top level procedures for retrieving documents
 *1 retrieve.top
 *2 * (unused1, unused2, inst)
 *3    char *unused1;
 *3    char *unused2;
 *3    int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures are top-level procedures which retrieve documents
 *7 for a set of queries.
 *7 As top-level procedures, they are responsible for setting
 *7 trace conditions, and for determining other execution time constraints,
 *7 such as when execution should stop (eg, if global_end is exceeded).
 *7 Return UNDEF if error, else 1.
 *8 Current possibilities are "retrieve", "retrieve_all", "ret_fdbk" 
***********************************************************************/
extern int init_retrieve(), retrieve(), close_retrieve();
extern int init_retrieve_all(), retrieve_all(), close_retrieve_all();
extern int init_ret_fdbk(), ret_fdbk(), close_ret_fdbk();
static PROC_TAB proc_topretrieve[] = {
    {"retrieve",     init_retrieve,  retrieve,      close_retrieve},
    {"retrieve_all", init_retrieve_all,retrieve_all,close_retrieve_all},
    {"ret_fdbk",     init_ret_fdbk,  ret_fdbk,      close_ret_fdbk},
};
static int num_proc_topretrieve =
    sizeof (proc_topretrieve) / sizeof (proc_topretrieve[0]);


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures to match a query against a collection
 *1 retrieve.coll_sim
 *2 * (in_retrieval, results, inst)
 *3   RETRIEVAL *in_retrieval;
 *3   RESULT *results;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures take an input query (possibly along with a list of
 *7 docs NOT to retrieve) and compute the similarity between the query and
 *7 all docs in the collection.  Results->full_results gives all similarities,
 *7 results->top_results gives the top documents retrieved.
 *7 Return UNDEF if error, else 1.
 *8 Current possibilities are "inverted" (or "vec_inv"), "seq",
 *8 "req_inverted", "text"
***********************************************************************/
/* Run a vector against entire collection */
extern int init_sim_vec_inv(), sim_vec_inv(), close_sim_vec_inv();
extern int init_sim_vec_invpos(), sim_vec_invpos(), close_sim_vec_invpos();
extern int init_sim_seq(), sim_seq(), close_sim_seq();
extern int init_sim_text(), sim_text(), close_sim_text();
extern int init_sim_vec_inv_lm(), sim_vec_inv_lm(), close_sim_vec_inv_lm(); 
extern int init_sim_vec_inv_lm_nsim(), sim_vec_inv_lm_nsim(), close_sim_vec_inv_lm_nsim(); 
static PROC_TAB proc_coll_sim[] = {
    {"inverted",    init_sim_vec_inv,   sim_vec_inv,   close_sim_vec_inv},
    {"vec_inv",		init_sim_vec_inv,   sim_vec_inv,   close_sim_vec_inv},
    {"vec_invpos",	init_sim_vec_invpos,   sim_vec_invpos,   close_sim_vec_invpos},
    {"inverted_lm", init_sim_vec_inv_lm, sim_vec_inv_lm, close_sim_vec_inv_lm},
    {"inverted_lm_nsim", init_sim_vec_inv_lm_nsim, sim_vec_inv_lm_nsim, close_sim_vec_inv_lm_nsim},
    {"vec_inv_lm", init_sim_vec_inv_lm, sim_vec_inv_lm, close_sim_vec_inv_lm},
    {"seq",		init_sim_seq,       sim_seq,       close_sim_seq},
    {"text",		init_sim_text,      sim_text,      close_sim_text},
    };
static int num_proc_coll_sim = sizeof (proc_coll_sim) / sizeof (proc_coll_sim[0]);

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures to match a query ctype against collection
 *1 retrieve.ctype_coll
 *2 * (qvec, results, inst)
 *3   VEC *qvec;
 *3   RESULT *results;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures take a user query, assumed to be one ctype, and
 *7 compute the similarities between the query and
 *7 all docs in the collection.  Results->full_results is incremented by
 *7 the similarity computed for each document.
 *7 results->top_results is updated, giving the top documents retrieved.
 *7 Return UNDEF if error, else 1.
 *8 Current possibilities are "ctype_inv"
***********************************************************************/
/* Run a ctype from vector against entire collection */
extern int init_sim_ctype_inv(), sim_ctype_inv(), close_sim_ctype_inv();
extern int init_sim_ctype_invpos(), sim_ctype_invpos(), close_sim_ctype_invpos();
extern int init_sim_ctype_inv_avgpos(), sim_ctype_inv_avgpos(), close_sim_ctype_inv_avgpos();
extern int init_sim_ctype_invpos_noprior(), sim_ctype_invpos_noprior(), close_sim_ctype_invpos_noprior();
extern int init_sim_ctype_inv_lm(), sim_ctype_inv_lm(), close_sim_ctype_inv_lm();
extern int init_sim_ctype_inv_lm_lda(), sim_ctype_inv_lm_lda(), close_sim_ctype_inv_lm_lda();
extern int init_sim_ctype_inv_lm_nsim(), sim_ctype_inv_lm_nsim(), close_sim_ctype_inv_lm_nsim();
extern int init_sim_ctype_inv_lm_aug(), sim_ctype_inv_lm_aug(), close_sim_ctype_inv_lm_aug();
extern int init_sim_ctype_inv_lm_aug_fast(), sim_ctype_inv_lm_aug_fast(), close_sim_ctype_inv_lm_aug_fast();
extern int init_sim_ctype_inv_lm_len(), sim_ctype_inv_lm_len(), close_sim_ctype_inv_lm_len();

static PROC_TAB proc_ctype_coll[] = {
    {"ctype_inv",	init_sim_ctype_inv, sim_ctype_inv, close_sim_ctype_inv},
    {"ctype_invpos", init_sim_ctype_invpos, sim_ctype_invpos, close_sim_ctype_invpos},
    {"ctype_invavgpos", init_sim_ctype_inv_avgpos, sim_ctype_inv_avgpos, close_sim_ctype_inv_avgpos},
    {"ctype_invpos_noprior", init_sim_ctype_invpos_noprior, sim_ctype_invpos_noprior, close_sim_ctype_invpos_noprior},
    {"ctype_inv_lm", init_sim_ctype_inv_lm, sim_ctype_inv_lm, close_sim_ctype_inv_lm},
    {"ctype_inv_lm_lda", init_sim_ctype_inv_lm_lda, sim_ctype_inv_lm_lda, close_sim_ctype_inv_lm_lda},
    {"ctype_inv_lm_nsim", init_sim_ctype_inv_lm_nsim, sim_ctype_inv_lm_nsim, close_sim_ctype_inv_lm_nsim},
    {"ctype_inv_lm_aug", init_sim_ctype_inv_lm_aug, sim_ctype_inv_lm_aug, close_sim_ctype_inv_lm_aug},
    {"ctype_inv_lm_aug_fast", init_sim_ctype_inv_lm_aug_fast, sim_ctype_inv_lm_aug_fast, close_sim_ctype_inv_lm_aug_fast},
    {"ctype_inv_lm_len", init_sim_ctype_inv_lm_len, sim_ctype_inv_lm_len, close_sim_ctype_inv_lm_len}
    };
static int num_proc_ctype_coll =
    sizeof (proc_ctype_coll) / sizeof (proc_ctype_coll[0]);


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures to match a vector against a vector
 *1 retrieve.vec_vec
 *2 * (vec_pair, sim_ptr, inst)
 *3   VEC_PAIR *vec_pair;
 *3   float *sim_ptr;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures take a vector pair and computes the similarity between
 *7 the two vectors, returning the value in sim_ptr.
 *7 Return UNDEF if error, else 1.
 *8 Current possibilities are "vec_vec"
***********************************************************************/
/* Run a vector against a vector, returning a sim */
extern int init_sim_vec_vec(), sim_vec_vec(), close_sim_vec_vec();
static PROC_TAB proc_vec_vec[] = {
    {"vec_vec",		init_sim_vec_vec, sim_vec_vec,	close_sim_vec_vec},
    };
static int num_proc_vec_vec =
    sizeof (proc_vec_vec) / sizeof (proc_vec_vec[0]);


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures to match a query against a vector
 *1 retrieve.q_vec
 *2 * (q_vec_pair, q_vec_result, inst)
 *3   Q_VEC_PAIR *q_vec_pair;
 *3   Q_VEC_RESULT *q_vec_result;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures take a query (of unknown structure) and a document 
 *7 and computes the similarity between the two, returning the value in 
 *7 q_vec_result, along with a possible explanation.
 *7 Return UNDEF if error, else 1.
 *8 Current possibilities are "vec_vec"
***********************************************************************/
/* Run a vector against a vector, returning a sim */
extern int init_sim_q_vec(), sim_q_vec(), close_sim_q_vec();
static PROC_TAB proc_q_vec[] = {
    {"vec_vec",		init_sim_q_vec,sim_q_vec,	close_sim_q_vec},
    };
static int num_proc_q_vec =
    sizeof (proc_q_vec) / sizeof (proc_q_vec[0]);


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures to match a vector ctype against a vector ctype
 *1 retrieve.ctype_vec
 *2 sim_inner (vec_pair, sim_ptr, inst)
 *3   VEC_PAIR *vec_pair;
 *3   float *sim_ptr;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures take a vector pair and computes the similarity between
 *7 the two vectors, returning the value in sim_ptr.  Each vector is assumed
 *7 to be a single ctype.
 *7 Return UNDEF if error, else 1.
 *8 Current possibilities are "inner"
***********************************************************************/
/* Run a ctype from vector against a ctype from a vector, returning a sim */
extern int init_sim_inner(), sim_inner(), close_sim_inner();
extern int init_sim_inner_lm(), sim_inner_lm(), close_sim_inner_lm();
static PROC_TAB proc_ctype_vec[] = {
    {"inner",		init_sim_inner,sim_inner,	close_sim_inner},
    {"lm",	init_sim_inner_lm, sim_inner_lm,	close_sim_inner_lm},
    };
static int num_proc_ctype_vec =
    sizeof (proc_ctype_vec) / sizeof (proc_ctype_vec[0]);

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures to match a vector list against a vector list
 *1 retrieve.vecs_vecs
 *2 * (vec_list_pair, results , inst)
 *3   VEC_LIST_PAIR *vec_list_pair;
 *3   ALT_RESULTS *results;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures take a vector list pair.
 *7 Each vector in vec_list_pair->vec_list1 is compared against every
 *7 vector in vec_list_pair->vec_list2.  All non-zero similarities are
 *7 returned in results. 
 *7 Return UNDEF if error, else 1.
 *8 Current possibilities are "vecs_vecs"
***********************************************************************/
/* Run a group of vectors against a group of vectors, returning all sims */
extern int init_vecs_vecs(), vecs_vecs(), close_vecs_vecs();
static PROC_TAB proc_vecs_vecs[] = {
    {"vecs_vecs",	init_vecs_vecs,	vecs_vecs,	close_vecs_vecs},
    };
static int num_proc_vecs_vecs =
    sizeof (proc_vecs_vecs) / sizeof (proc_vecs_vecs[0]);

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures to maintain a list of top-ranked docs
 *1 retrieve.rank_tr
 *2 * (new, results, inst)
 *3   TOP_RESULT *new;
 *3   RESULT *results;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 Add the new document to results.top_results if the similarity is
 *7 high enough.  Break ties among similarities by procedure 
 *7 specific means.
 *7 Return UNDEF if error, 0 if not added, else 1.
 *8 Current possibilities are "rank_did"
***********************************************************************/
extern int rank_did();
extern int rank_did_nsim();
static PROC_TAB proc_rank_tr[] = {
    {"rank_did",		INIT_EMPTY,	rank_did,	CLOSE_EMPTY},
    {"rank_did_nsim",	INIT_EMPTY,	rank_did_nsim,	CLOSE_EMPTY},
    };
static int num_proc_rank_tr = sizeof (proc_rank_tr) /
         sizeof (proc_rank_tr[0]);


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures to get the next query for a retrieval
 *1 retrieve.get_query
 *2 * (unused, query_vec, inst)
 *3   char *unused;
 *3   QUERY_VECTOR *query_vec;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 Return the next query for a retrieval in query_vec.
 *7 Return UNDEF if error, 0 if no more queries, else 1.
 *8 Current possibilities are "get_q_vec", "get_q_text", "get_q_user", 

***********************************************************************/
extern int init_get_q_vec(), get_q_vec(), close_get_q_vec();
extern int init_get_q_text(), get_q_text(), close_get_q_text();
extern int init_get_q_user(), get_q_user(), close_get_q_user();
static PROC_TAB proc_get_query[] = {
    {"get_q_vec",	init_get_q_vec,	get_q_vec,	close_get_q_vec},
    {"get_q_text",	init_get_q_text,get_q_text,	close_get_q_text},
    /*    {"get_q_user",	init_get_q_user,get_q_user,	close_get_q_user},*/
    };
static int num_proc_get_query = sizeof (proc_get_query) /
         sizeof (proc_get_query[0]);


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures to index a new query from the user
 *1 retrieve.query_index
 *2 * (textdisp, query_vec, inst)
 *3   SM_TEXTDISPLAY *textdisp;
 *3   QUERY_VECTOR *query_vec;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 Take the location of a query file obtained from a user in textdisp,
 *7 and return the indexed query in query_vec
 *7 Return UNDEF if error, 0 if query is empty, else 1.
 *8 Current possibilities are "std_vec"
***********************************************************************/
extern int init_get_q_text(), get_q_text(), close_get_q_text();
static PROC_TAB proc_query_index[] = {
    {"std_vec", init_get_q_text,   get_q_text,	close_get_q_text},
    };
static int num_proc_query_index = sizeof (proc_query_index) /
         sizeof (proc_query_index[0]);


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures to output results of a retrieval
 *1 retrieve.output
 *2 * (results, tr_vec, inst)
 *3   RESULT *results;
 *3   TR_VEC *tr_vec;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 Take the results of a retrieval in results, and output in appropriate
 *7 form.  If possible, also add the top-ranked results to tr_vec
 *7 Return UNDEF if error, else 1.
 *8 Current possibilities are "ret_tr", "ret_rr", "ret_tr_rr",
***********************************************************************/
extern int init_ret_tr(), ret_tr(), close_ret_tr();
extern int init_ret_tr_rr(), ret_tr_rr(), close_ret_tr_rr();
static PROC_TAB proc_r_output[] = {
    {"ret_tr",		init_ret_tr,	ret_tr,		close_ret_tr},
    {"ret_tr_rr",	init_ret_tr_rr,	ret_tr_rr,      close_ret_tr_rr},
    };
static int num_proc_r_output = sizeof (proc_r_output) /
         sizeof (proc_r_output[0]);

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures to get previously seen docs for a retrieval
 *1 retrieve.get_seen
 *2 * (query_vec, tr_vec, inst)
 *3   QUERY_VECTOR *query_vec;
 *3   TR_VEC *tr_vec;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 Take a query id given by query_vec, and find all docs that are not to be
 *7 retrieved for that query, returning them in tr_vec.  Normally, these docs
 *7 have already been seen by the user, who won't want to see them again.
 *7 Return UNDEF if error, else 1.
 *8 Current possibilities are "get_seen_docs"
***********************************************************************/
extern int init_get_seen_docs(), get_seen_docs(), close_get_seen_docs();
static PROC_TAB proc_get_seen_docs[] = {
    {"get_seen_docs",	init_get_seen_docs,get_seen_docs,close_get_seen_docs},
    {"empty",		INIT_EMPTY,	EMPTY,	CLOSE_EMPTY},
    };
static int num_proc_get_seen_docs = sizeof (proc_get_seen_docs) /
         sizeof (proc_get_seen_docs[0]);

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: table of other hierarchy tables for retrieval procedures
 *1 retrieve
 *7 Procedure table mapping a string procedure table name to either another
 *7 table of hierarchy tables, or to a stem table which maps
 *7 names to procedures
 *8 Current possibilities are "coll_sim", "ctype_coll", "vec_vec",
 *8 "ctype_vec", "vecs_vecs", "rank_tr", "get_query",
 *8 "user_query", "query_index", "output", "get_seen_docs"
***********************************************************************/
TAB_PROC_TAB proc_retrieve[] = {
    {"top",	TPT_PROC, NULL,	proc_topretrieve,    	&num_proc_topretrieve},
    {"coll_sim",	TPT_PROC, NULL,	proc_coll_sim,    	&num_proc_coll_sim},
    {"ctype_coll",TPT_PROC, NULL,proc_ctype_coll,	&num_proc_ctype_coll},
    {"vec_vec",  TPT_PROC, NULL,	proc_vec_vec,  		&num_proc_vec_vec},
    {"q_vec",  TPT_PROC, NULL,	proc_q_vec,  		&num_proc_q_vec},
    {"ctype_vec",TPT_PROC, NULL, proc_ctype_vec,		&num_proc_ctype_vec},
    {"vecs_vecs",TPT_PROC, NULL, proc_vecs_vecs,		&num_proc_vecs_vecs},
    {"rank_tr",	TPT_PROC, NULL,	proc_rank_tr,     	&num_proc_rank_tr},
    {"get_query",TPT_PROC, NULL,	proc_get_query,   	&num_proc_get_query},
    {"query_index",TPT_PROC, NULL,proc_query_index,   	&num_proc_query_index},
    {"output",	TPT_PROC, NULL,	proc_r_output,    	&num_proc_r_output},
    {"get_seen_docs",TPT_PROC, NULL,proc_get_seen_docs, &num_proc_get_seen_docs},
  };

int num_proc_retrieve = sizeof (proc_retrieve) / sizeof (proc_retrieve[0]);
