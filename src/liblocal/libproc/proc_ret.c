#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libproc/proc_ret.c,v 11.2 1993/02/03 16:52:20 smart Exp $";
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
 *0 Local hierarchy table giving top level procedures for retrieving documents
 *1 local.retrieve.top
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
 *8 Current possibilities are "retrieve_seq", "retrieve_seen"
***********************************************************************/
extern int init_retrieve_seen(), retrieve_seen(), close_retrieve_seen();
/* extern int init_learn_seen(), learn_seen(), close_learn_seen(); */
/* extern int init_learn2_seen(), learn2_seen(), close_learn2_seen(); */
/* extern int init_learn5_seen(), learn5_seen(), close_learn5_seen(); */
/* extern int init_learn_seendoc(), learn_seendoc(), close_learn_seendoc(); */
extern int init_learn_seendoc_query(), learn_seendoc_query(), close_learn_seendoc_query();
extern int init_retrieve_seq(), retrieve_seq(), close_retrieve_seq();
extern int init_ret_af(), ret_af(), close_ret_af();
extern int init_learn_ret_af(), learn_ret_af(), close_learn_ret_af();
static PROC_TAB proc_topretrieve[] = {
    {"retrieve_seen",init_retrieve_seen,retrieve_seen, close_retrieve_seen},
/*     {"learn_seen",init_learn_seen,learn_seen, close_learn_seen}, */
/*     {"learn2_seen",init_learn2_seen,learn2_seen, close_learn2_seen}, */
/*     {"learn5_seen",init_learn5_seen,learn5_seen, close_learn5_seen}, */
/*     {"learn_seendoc",init_learn_seendoc,learn_seendoc, close_learn_seendoc}, */
    {"learn_seendoc_query",init_learn_seendoc_query,learn_seendoc_query,
         close_learn_seendoc_query},
    {"retrieve_seq", init_retrieve_seq, retrieve_seq,  close_retrieve_seq},
    {"retrieve_af", init_ret_af, ret_af,  close_ret_af},
    {"learn_af", init_learn_ret_af, learn_ret_af,  close_learn_ret_af},
};
static int num_proc_topretrieve =
    sizeof (proc_topretrieve) / sizeof (proc_topretrieve[0]);


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Local hierarchy table: procedures to match a query against a collection
 *1 local.retrieve.coll_sim
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
 *8 Current possibilities are  NONE
***********************************************************************/
/* Run a vector against entire collection */
extern int init_sim_twopass(), sim_twopass(), close_sim_twopass(); 
static PROC_TAB proc_coll_sim[] = { 
    {"sim_twopass", init_sim_twopass, sim_twopass, close_sim_twopass},
    };
static int num_proc_coll_sim = sizeof (proc_coll_sim) / sizeof (proc_coll_sim[0]); 


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Local Hierarchy table: procedures to match a query ctype against collection
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
 *8 Current possibilities are sim_cty_idf().
***********************************************************************/
/* Run a ctype from vector against entire collection */
extern int init_sim_ctype_qtc(), sim_ctype_qtc(), close_sim_ctype_qtc(); 
extern int init_sim_ctype_idf(), sim_ctype_idf(), close_sim_ctype_idf(); 
extern int init_sim_ctype_inv_heap(), sim_ctype_inv_heap(), close_sim_ctype_inv_heap(); 
extern int init_sim_ctype_coocc(), sim_ctype_coocc(), close_sim_ctype_coocc(); 

static PROC_TAB proc_ctype_coll[] = { 
    {"ctype_inv_heap", init_sim_ctype_inv_heap, sim_ctype_inv_heap, close_sim_ctype_inv_heap}, 
    {"ctype_qtc", init_sim_ctype_qtc, sim_ctype_qtc, close_sim_ctype_qtc}, 
    {"ctype_idf", init_sim_ctype_idf, sim_ctype_idf, close_sim_ctype_idf}, 
    {"ctype_coocc", init_sim_ctype_coocc, sim_ctype_coocc, close_sim_ctype_coocc}, 
}; 
static int num_proc_ctype_coll = 
    sizeof (proc_ctype_coll) / sizeof (proc_ctype_coll[0]); 

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Local hierarchy table: procedures to return the next doc vec to be searched
 *1 local.retrieve.get_doc
 *2 * (NULL, vec, inst)
 *3    char *NULL;
 *3    VEC *vec;
 *3    int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 These procedures get the next document vector in a sequential search
 *7 Return UNDEF if error, else 1.
 *8 Current possibilities are  NONE "textloc", "vec", "wtvec"
***********************************************************************/
/* Return the next doc to be searched */
extern int init_getdoc_textloc(), getdoc_textloc(), close_getdoc_textloc();
extern int init_getdoc_vec(), getdoc_vec(), close_getdoc_vec();
extern int init_getdoc_wtvec(), getdoc_wtvec(), close_getdoc_wtvec();
static PROC_TAB proc_get_doc[] = {
    {"textloc", init_getdoc_textloc, getdoc_textloc, close_getdoc_textloc},
    {"vec", init_getdoc_vec, getdoc_vec, close_getdoc_vec},
    {"wtvec", init_getdoc_wtvec, getdoc_wtvec, close_getdoc_wtvec},
    };
static int num_proc_get_doc =
    sizeof (proc_get_doc) / sizeof (proc_get_doc[0]);

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Local hierarchy table: procedures to match a vector against a vector
 *1 local.retrieve.vec_vec
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
 *8 Current possibilities are  NONE "trec_thresh", "trec_thresh2", "top_parts"
***********************************************************************/
/* Run a vector against a vector, returning a sim */
/*extern int init_trec_thresh(), trec_thresh(), close_trec_thresh(); */
/*extern int init_trec_thresh2(), trec_thresh2(), close_trec_thresh2(); */
/*extern int init_piece_extract(), piece_extract(), close_piece_extract(); */
/*extern int init_piece_segment(), piece_segment(), close_piece_segment(); */
/*extern int init_piece_conti(), piece_conti(), close_piece_conti(); */
/*extern int init_piece_best(), piece_best(), close_piece_best(); */
/*extern int init_hierarchical(), hierarchical(), close_hierarchical(); */
extern int init_vec_vec_match(), vec_vec_match(), close_vec_vec_match();
extern int init_vec_vec_coocc(), vec_vec_coocc(), close_vec_vec_coocc();
static PROC_TAB proc_vec_vec[] = {
/*    "trec_thresh", init_trec_thresh, trec_thresh, close_trec_thresh, */
/*    "trec_thresh2", init_trec_thresh2, trec_thresh2, close_trec_thresh2, */
/*    "piece_extract", init_piece_extract, piece_extract, close_piece_extract, */
/*    "piece_segment", init_piece_segment, piece_segment, close_piece_segment, */
/*    "piece_conti", init_piece_conti, piece_conti, close_piece_conti, */
/*    "piece_best", init_piece_best, piece_best, close_piece_best, */
/*    "hierarchical", init_hierarchical, hierarchical, close_hierarchical, */
    {"match", init_vec_vec_match, vec_vec_match, close_vec_vec_match},
    {"coocc", init_vec_vec_coocc, vec_vec_coocc, close_vec_vec_coocc}
    };
static int num_proc_vec_vec =
    sizeof (proc_vec_vec) / sizeof (proc_vec_vec[0]);

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures to match a vector ctype against a vector ctype
 *1 local.retrieve.ctype_vec
 *2 sim_filter (vec_pair, sim_ptr, inst)
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
 *8 Current possibilities are "filter"
***********************************************************************/
/* Run a ctype from vector against a ctype from a vector, returning a sim */
extern int init_sim_filter(), sim_filter(), close_sim_filter();
static PROC_TAB proc_ctype_vec[] = {
    {"filter",	init_sim_filter,	sim_filter,	close_sim_filter},
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
/*extern int init_mixed_cmpr(), mixed_cmpr(), close_mixed_cmpr(); */
/*static PROC_TAB proc_vecs_vecs[] = { */
/*    "mixed_cmpr", init_mixed_cmpr, mixed_cmpr, close_mixed_cmpr, */
/*    }; */
/*static int num_proc_vecs_vecs = */
/*    sizeof (proc_vecs_vecs) / sizeof (proc_vecs_vecs[0]); */


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Local hierarchy table: procedure to assign sims to docs based on local sims
 *1 local.retrieve.local_comb
 *2 * (parts_results, results, inst)
 *3   TOP_RESULT *local_top_results;
 *3   RESULT *results;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 Combining global similarity (from results->top_results) and local 
 *7 similarity (eg paragraphs) from local_top_results of docs 
 *7 to query, assign final similarity to docs in results->top_results. 
 *7 Return UNDEF if error, 0 if not added, else 1.
 *8 Current possibilities are "req_save"
***********************************************************************/
/*extern int init_local_comb_weight(), local_comb_weight(), close_local_comb_weight(); */
/*extern int init_local_comb_weight_norm(), local_comb_weight_norm(), close_local_comb_weight_norm(); */
/*extern int init_local_comb_weight_normabs(), local_comb_weight_normabs(), close_local_comb_weight_normabs(); */
/*static PROC_TAB proc_local_comb[] = { */
/*    "weight",     init_local_comb_weight, local_comb_weight, close_local_comb_weight, */
/*    "weight_norm",     init_local_comb_weight_norm, local_comb_weight_norm, close_local_comb_weight_norm, */
/*    "weight_normabs",     init_local_comb_weight_normabs, local_comb_weight_normabs, close_local_comb_weight_normabs, */
/*    }; */
/*static int num_proc_local_comb = sizeof (proc_local_comb) / */
/*         sizeof (proc_local_comb[0]); */


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Local hierarchy table: handle query-doc mixed output restrictions
 *1 local.retrieve.mixed.mixed
 *2 * (orig, new, inst)
 *3   ALT_RESULTS *orig;
 *3   ALT_RESULTS *new;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Changes the 'orig' results to the 'new' results based on several
 *7 sets of restrictions.
 *7 Return UNDEF if error, 0 if not added, else 1.
 *8 Current possibilities are "mixed".
***********************************************************************/
/*extern int init_mixed(), mixed(), close_mixed(); */
/*static PROC_TAB proc_mixed[] = { */
/*    "mixed",	init_mixed,	mixed,	close_mixed, */
/*    }; */
/*static int num_proc_mixed = sizeof (proc_mixed) / */
/*         sizeof (proc_mixed[0]); */


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Local hierarchy table: procedures to maintain a list of top-ranked docs
 *1 local.retrieve.rank_tr
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
 *8 Current possibilities are "rank_trec_did"
***********************************************************************/
/*extern int rank_trec_did(); */
/*static PROC_TAB proc_rank_tr[] = { */
/*    "rank_trec_did",    INIT_EMPTY,     rank_trec_did,  CLOSE_EMPTY, */
/*    }; */
/*static int num_proc_rank_tr = sizeof (proc_rank_tr) / */
/*         sizeof (proc_rank_tr[0]); */


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Local hierarchy table: handle query-doc mixed output restrictions
 *1 local.retrieve.mixed.explode
 *2 * (did, vec_list, inst)
 *3   long did;
 *3   VEC_LIST *vec_list;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Break 'did' into parts and store in 'vec_list'
 *7 Return UNDEF if error, 0 if not added, else 1.
 *8 Current possibilities are "explode".
***********************************************************************/
/*extern int init_explode(), explode(), close_explode(); */
/*extern int init_mixed_expw(), mixed_expw(), close_mixed_expw(); */
/*static PROC_TAB proc_explode[] = { */
/*    "explode",	init_explode, explode, close_explode, */
/*    "mixed_expw", init_mixed_expw, mixed_expw, close_mixed_expw, */
/*    }; */
/*static int num_proc_explode = sizeof (proc_explode) / */
/*         sizeof (proc_explode[0]); */

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Local hierarchy table: filter results of a comparison
 *1 local.retrieve.resfilter.resfilter
 *2 * (in_altres, global_sim, inst)
 *3   ALT_RESULTS in_altres;
 *3   float global_sim;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Apply various restriction on results of a comparison
 *7 Return UNDEF if error, 0 if not added, else 1.
 *8 Current possibilities are "resfilter".
***********************************************************************/
/*extern int init_resfilter(), resfilter(), close_resfilter(); */
/*static PROC_TAB proc_resfilter[] = { */
/*    "resfilter",	init_resfilter, resfilter, close_resfilter, */
/*    }; */
/*static int num_proc_resfilter = sizeof (proc_resfilter) / */
/*         sizeof (proc_resfilter[0]); */

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Local hierarchy table: perform single query retrieval on seen docs
 *1 local.retrieve.q_tr
 *2 * (seen_vec, results, inst)
 *3   TR_VEC *seen_vec;
 *3   RESULT *results;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Perform a single query retrieval, comparing query (seen_vec->qid)
 *7 against all documents in seen_vec->tr.  Top_results given in results.
 *7 Return UNDEF if error, 0 if not added, else 1.
 *8 Current possibilities are "q_tr_index"
***********************************************************************/
/* extern int init_q_tr_qindex(), q_tr_qindex(), close_q_tr_qindex();  */
/* extern int init_q_tr_qindex_merge(), q_tr_qindex_merge(), close_q_tr_qindex_merge();  */
extern int init_q_tr_wtdoc(), q_tr_wtdoc(), close_q_tr_wtdoc();
extern int init_q_tr_prox(), q_tr_prox(), close_q_tr_prox();
/* extern int init_q_tr_bool(), q_tr_bool(), close_q_tr_bool(); */
extern int init_q_tr_softbool(), q_tr_softbool(), close_q_tr_softbool();
extern int init_q_tr_coocc(), q_tr_coocc(), close_q_tr_coocc();
extern int init_q_tr_winmatch(), q_tr_winmatch(), close_q_tr_winmatch();
extern int init_q_tr_cl_sa(), q_tr_cl_sa(), close_q_tr_cl_sa();
static PROC_TAB proc_q_tr[] = { 
/*     {"index",	init_q_tr_qindex, q_tr_qindex, close_q_tr_qindex}, */
/*     {"index_merge",	init_q_tr_qindex_merge, q_tr_qindex_merge, close_q_tr_qindex_merge}, */
	{"wtdoc",	init_q_tr_wtdoc, q_tr_wtdoc, close_q_tr_wtdoc},
	{"prox",	init_q_tr_prox, q_tr_prox, close_q_tr_prox},
/*	{"bool",	init_q_tr_bool, q_tr_bool, close_q_tr_bool}, */
	{"softbool",	init_q_tr_softbool, q_tr_softbool, close_q_tr_softbool},
	{"coocc",	init_q_tr_coocc, q_tr_coocc, close_q_tr_coocc},
	{"winmatch",	init_q_tr_winmatch, q_tr_winmatch, close_q_tr_winmatch},
	{"cluster_selall",	init_q_tr_cl_sa, q_tr_cl_sa, close_q_tr_cl_sa},
    };
static int num_proc_q_tr = sizeof (proc_q_tr) / 
         sizeof (proc_q_tr[0]); 

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Local hierarchy table: Get location info for terms and docs
 *1 local.retrieve.q_qdoc
 *2 * (seen_vec, qdoc_info, inst)
 *3   TR_VEC *seen_vec;
 *3   QDOC_INFO *qdoc_info
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Index a single query plus all retrieved docs (given by seen_vec).
 *7 Return info about location of all terms in doc that match query terms.
 *7 Return UNDEF if error, 0 if not added, else 1.
 *8 Current possibilities are "index", "store", "cache"
***********************************************************************/
/* extern int init_q_qd_index(), q_qd_index(), close_q_qd_index();  */
/* extern int init_q_qd_store(), q_qd_store(), close_q_qd_store();  */
/* extern int init_q_qd_cache(), q_qd_cache(), close_q_qd_cache();  */
static PROC_TAB proc_q_qdoc[] = { 
/*     {"index",	init_q_qd_index, q_qd_index, close_q_qd_index}, */
/*     {"store",	init_q_qd_store, q_qd_store, close_q_qd_store}, */
/*     {"cache",	init_q_qd_cache, q_qd_cache, close_q_qd_cache}, */
{"empty",            INIT_EMPTY,     EMPTY,          CLOSE_EMPTY}
    };
static int num_proc_q_qdoc = sizeof (proc_q_qdoc) / 
         sizeof (proc_q_qdoc[0]); 

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Local hierarchy table: Calculate distances from location info
 *1 local.retrieve.qdoc_qdis
 *2 * (qloc_array, qdis_array, inst)
 *3   QLOC_ARRAY *qloc_array;
 *3   QDIS_ARRAY *qdis_array;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 From a single vectors info about term location, calculate a distance
 *7 from beginning of record of each term (distance may depend on
 *7 things like number of paragraphs or sentences seen.)
 *7 Return UNDEF if error, 0 if not added, else 1.
 *8 Current possibilities are "token"
***********************************************************************/
extern int init_qloc_qdis_token(), qloc_qdis_token(), close_qloc_qdis_token(); 
static PROC_TAB proc_qloc_qdis[] = { 
/*     {"token",	init_qloc_qdis_token, qloc_qdis_token, close_qloc_qdis_token}, */
{"empty",            INIT_EMPTY,     EMPTY,          CLOSE_EMPTY}
    };
static int num_proc_qloc_qdis = sizeof (proc_qloc_qdis) / 
         sizeof (proc_qloc_qdis[0]); 

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Local hierarchy table: Calculate similarity based on term locality
 *1 local.retrieve.qdis_sim
 *2 * (qdis_ret, sim, inst)
 *3   QDIS_RET *qdis_ret;
 *3   float *sim;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Calculate a single vector similarity, given distances from beginning
 *7 of record for each matching term in query and doc.
 *7 Return UNDEF if error, 0 if not added, else 1.
 *8 Current possibilities are "null", "stat"
***********************************************************************/
/* extern int init_qdis_sim_null(), qdis_sim_null(), close_qdis_sim_null();  */
/* extern int init_qdis_sim_stat(), qdis_sim_stat(), close_qdis_sim_stat();  */
/* extern int init_qdis_sim_find_cooc(), qdis_sim_find_cooc(), close_qdis_sim_find_cooc();  */
/* extern int init_qdis_sim_window_stat(), qdis_sim_window_stat(), close_qdis_sim_window_stat(); */
/* extern int init_qdis_sim_1(), qdis_sim_1(), close_qdis_sim_1();  */
/* extern int init_qdis_sim_2(), qdis_sim_2(), close_qdis_sim_2();  */
/* extern int init_qdis_sim_3(), qdis_sim_3(), close_qdis_sim_3();  */
/* extern int init_qdis_sim_4(), qdis_sim_4(), close_qdis_sim_4();  */
/* extern int init_qdis_sim_5(), qdis_sim_5(), close_qdis_sim_5();  */
/* extern int init_qdis_sim_6(), qdis_sim_6(), close_qdis_sim_6();  */
/* extern int init_qdis_sim_7(), qdis_sim_7(), close_qdis_sim_7();  */
/* extern int init_qdis_sim_8(), qdis_sim_8(), close_qdis_sim_8();  */
/* extern int init_qdis_sim_9(), qdis_sim_9(), close_qdis_sim_9();  */
/* extern int init_qdis_sim_10(), qdis_sim_10(), close_qdis_sim_10();  */
/* extern int init_qdis_sim_11(), qdis_sim_11(), close_qdis_sim_11();  */
/* extern int init_qdis_sim_12(), qdis_sim_12(), close_qdis_sim_12();  */
/* extern int init_qdis_sim_13(), qdis_sim_13(), close_qdis_sim_13();  */
/* extern int init_qdis_sim_14(), qdis_sim_14(), close_qdis_sim_14();  */
/* extern int init_qdis_sim_c_1(), qdis_sim_c_1(), close_qdis_sim_c_1();  */
static PROC_TAB proc_qdis_sim[] = { 
/*     {"null",	init_qdis_sim_null, qdis_sim_null, close_qdis_sim_null}, */
/*     {"stat",	init_qdis_sim_stat, qdis_sim_stat, close_qdis_sim_stat}, */
/*     {"find_cooc",init_qdis_sim_find_cooc, qdis_sim_find_cooc, close_qdis_sim_find_cooc}, */
/*     {"wstat",init_qdis_sim_window_stat, qdis_sim_window_stat, close_qdis_sim_window_stat}, */
/*     {"c_1",     init_qdis_sim_c_1, qdis_sim_c_1, close_qdis_sim_c_1}, */
/*     {"1",	init_qdis_sim_1, qdis_sim_1, close_qdis_sim_1}, */
/*     {"2",	init_qdis_sim_2, qdis_sim_2, close_qdis_sim_2}, */
/*     {"3",	init_qdis_sim_3, qdis_sim_3, close_qdis_sim_3}, */
/*     {"4",	init_qdis_sim_4, qdis_sim_4, close_qdis_sim_4}, */
/*     {"5",	init_qdis_sim_5, qdis_sim_5, close_qdis_sim_5}, */
/*     {"6",	init_qdis_sim_6, qdis_sim_6, close_qdis_sim_6}, */
/*     {"7",	init_qdis_sim_7, qdis_sim_7, close_qdis_sim_7}, */
/*     {"8",	init_qdis_sim_8, qdis_sim_8, close_qdis_sim_8}, */
/*     {"9",	init_qdis_sim_9, qdis_sim_9, close_qdis_sim_9}, */
/*     {"10",	init_qdis_sim_10, qdis_sim_10, close_qdis_sim_10}, */
/*     {"11",	init_qdis_sim_11, qdis_sim_11, close_qdis_sim_11}, */
/*     {"12",	init_qdis_sim_12, qdis_sim_12, close_qdis_sim_12}, */
/*     {"13",	init_qdis_sim_13, qdis_sim_13, close_qdis_sim_13}, */
/*     {"14",	init_qdis_sim_14, qdis_sim_14, close_qdis_sim_14}, */
{"empty",            INIT_EMPTY,     EMPTY,          CLOSE_EMPTY}
    };
static int num_proc_qdis_sim = sizeof (proc_qdis_sim) / 
         sizeof (proc_qdis_sim[0]); 

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table: procedures to retrieve most similar sentences
    from top-ranked documents.
 *1 retrieve.sent_sim
 *2 * (Sentence* sentences, VEC* qvec)		// takes as input a list of sentenece objects and the query
 *7 Return UNDEF if error, else 1.
 *8 Current possibilities are "lm", "rlm" and "trlm"
***********************************************************************/
extern int init_sentsim_lm(), sentsim_lm(), close_sentsim_lm();
extern int init_sentsim_rlm(), sentsim_rlm(), close_sentsim_rlm();
extern int init_sentsim_trlm(), sentsim_trlm(), close_sentsim_trlm();
extern int init_sentsim_trlm_phrase(), sentsim_trlm_phrase(), close_sentsim_trlm_phrase();
extern int init_sentsim_lmtrlm(),  sentsim_lmtrlm(),   close_sentsim_lmtrlm();
static PROC_TAB proc_sent_sim[] = {
    {"lm",      init_sentsim_lm,   sentsim_lm,   close_sentsim_lm},
    {"rlm",		init_sentsim_rlm,   sentsim_rlm,   close_sentsim_rlm},
    {"trlm",	init_sentsim_trlm,   sentsim_trlm,   close_sentsim_trlm},
    {"trlm_phrase",	init_sentsim_trlm_phrase,   sentsim_trlm_phrase,   close_sentsim_trlm_phrase},
    {"lmtrlm",	init_sentsim_lmtrlm,   sentsim_lmtrlm,   close_sentsim_lmtrlm},
};
static int num_proc_sent_sim = sizeof (proc_sent_sim) / sizeof (proc_sent_sim[0]);

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Local Hierarchy table: table of other hierarchy tables for retrieval
 *1 local.retrieve
 *7 Procedure table mapping a string procedure table name to either another
 *7 table of hierarchy tables, or to a stem table which maps
 *7 names to procedures
 *8 Current possibilities are "top", "q_tr", "q_qdoc", "qloc_qdis", 
 *8 "qdis_sim"
***********************************************************************/
TAB_PROC_TAB lproc_retrieve[] = {
    {"top",      TPT_PROC, NULL, proc_topretrieve,     &num_proc_topretrieve},
    {"q_tr",     TPT_PROC, NULL, proc_q_tr,            &num_proc_q_tr},
    {"q_qdoc",   TPT_PROC, NULL, proc_q_qdoc,          &num_proc_q_qdoc},
    {"qloc_qdis",TPT_PROC, NULL, proc_qloc_qdis,       &num_proc_qloc_qdis},
    {"qdis_sim", TPT_PROC, NULL, proc_qdis_sim,        &num_proc_qdis_sim},
    {"coll_sim", TPT_PROC, NULL, proc_coll_sim,       &num_proc_coll_sim},
    {"ctype_coll",TPT_PROC, NULL,proc_ctype_coll,      &num_proc_ctype_coll}, 
    {"get_doc",  TPT_PROC, NULL, proc_get_doc,         &num_proc_get_doc}, 
    {"vec_vec",  TPT_PROC, NULL, proc_vec_vec,           &num_proc_vec_vec},
    {"ctype_vec",TPT_PROC, NULL, proc_ctype_vec,                &num_proc_ctype_vec},
    {"sent_sim",	TPT_PROC, NULL,	proc_sent_sim,    	&num_proc_sent_sim},	/* procedure to retrieve sentences from top-ranked docs */
/*    "rank_tr",  TPT_PROC, NULL, proc_rank_tr,           &num_proc_rank_tr, */
/*    "local_comb",TPT_PROC, NULL,proc_local_comb,        &num_proc_local_comb, */
/*    "mixed",	TPT_PROC, NULL,	proc_mixed,	&num_proc_mixed, */
/*    "explode",	TPT_PROC, NULL,	proc_explode,	&num_proc_explode, */
/*    "resfilter",TPT_PROC, NULL,	proc_resfilter,	&num_proc_resfilter, */
/*    "vecs_vecs",TPT_PROC, NULL,	proc_vecs_vecs,	&num_proc_vecs_vecs, */
  };

int num_lproc_retrieve = sizeof (lproc_retrieve) / sizeof (lproc_retrieve[0]);
