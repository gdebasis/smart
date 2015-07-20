#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libretrieve/retrieve.c,v 11.2 1993/02/03 16:54:27 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Top-level procedure to run query collection against incoming text docs
 *1 local.retrieve.top.seq
 *2 retrieve_seq (unused1, unused2, inst)
 *3   char *unused1;
 *3   char *unused2;
 *3   int inst;
 *4 init_retrieve_seq (spec, unused)
 *5   "retrieve.get_query"
 *5   "retrieve.get_doc"
 *5   "retrieve.get_seen_docs"
 *5   "retrieve.qrels_file"
 *5   "retrieve.qrels_file.rmode"
 *5   "retrieve.trace"
 *4 close_retrieve_seq (inst)
 *6 global_start, global_end matched against query_id to determine if
 *6 this query should be run.

 *7 Top-level procedure to run retrievals with the specified indexed
 *7 query collection on the specified indexed document collection.
 *9 For time being, assume tr results only?  Ignore seen_docs?
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "context.h"
#include "retrieve.h"
#include "rr_vec.h"
#include "vector.h"

/* Single procedure to run a retrieval with the specified indexed
   query collection on the specified documents

   Steps are:
   get all queries
   sequentially go through docs one at a time, compare each doc against all
   the queries.
*/


static char *pp_infile;          /* Location of files to index */
static PROC_INST pp,             /* Preparse procedures */
    next_vecid,                  /* Determine next valid id to use */
    index_pp;                    /* Convert preparsed text to vector */
static PROC_INST get_query,
    get_doc,
    vec_vec,
    store_results;
static char *qrels_file;
static long qrels_mode;
static long num_wanted;
static long start_did, end_did;

static SPEC_PARAM spec_args[] = {
    {"index.preparse.doc_loc",  getspec_string,(char *) &pp_infile},
    {"index.doc.preparse",      getspec_func, (char *) &pp.ptab},
    {"index.doc.next_vecid",    getspec_func, (char *) &next_vecid.ptab},
    {"index.doc.index_pp",      getspec_func, (char *) &index_pp.ptab},

    {"retrieve.get_query",        getspec_func, (char *) &get_query.ptab},
    {"retrieve.get_doc",          getspec_func, (char *) &get_doc.ptab},
    {"retrieve.vec_vec",          getspec_func, (char *) &vec_vec.ptab},
    {"retrieve.output",           getspec_func, (char *) &store_results.ptab},
    {"retrieve.qrels_file",       getspec_dbfile,(char *) &qrels_file},
    {"retrieve.qrels_file.rmode", getspec_filemode,(char *) &qrels_mode},
    {"retrieve.num_wanted",       getspec_long,(char *) &num_wanted},
    {"retrieve.start_did",        getspec_long,(char *) &start_did},
    {"retrieve.end_did",          getspec_long,(char *) &end_did},
    TRACE_PARAM ("retrieve.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static int qrels_fd;

typedef struct {
    VEC vec;
    RESULT result;
    long min_sim_index;
} QUERY_LIST;

static QUERY_LIST *query_list;
static long max_query_list = 0;
static long num_queries;
#define QUERY_INCR 200

static int add_new_query();
static void store_top();
static int comp_top_sim ();

int
init_retrieve_seq (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_retrieve_seq");

    /* Call all initialization procedures */


    if (UNDEF == (get_doc.inst =
                  get_doc.ptab->init_proc (spec, NULL)) ||
        UNDEF == (get_query.inst =
                  get_query.ptab->init_proc (spec, NULL)) ||
        UNDEF == (vec_vec.inst =
                  vec_vec.ptab->init_proc (spec, NULL)) ||
        UNDEF == (store_results.inst =
                  store_results.ptab->init_proc (spec, NULL))) {
        return (UNDEF);
    }

    qrels_fd = UNDEF;
    if (VALID_FILE (qrels_file)) {
        if (UNDEF == (qrels_fd = open_rr_vec (qrels_file, qrels_mode)))
            return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving init_retrieve_seq");
    return (0);
}

int
retrieve_seq (unused1, unused2, inst)
char *unused1;
char *unused2;
int inst;
{
    int status;
    QUERY_VECTOR query_vec;
    TR_VEC seendoc_vec;
    RR_VEC rr_vec;
    VEC vec;
    VEC_PAIR vec_pair;
    long i, did;
    float sim;

    PRINT_TRACE (2, print_string, "Trace: entering retrieve_seq");

    /* Get each query, and reserve space for its results */
    num_queries = 0;
    while (1 == (status = get_query.ptab->proc (NULL,
                                                &query_vec,
                                                get_query.inst)))  {
        if (query_vec.qid < global_start)
            continue;
        /* Check to see if any rel docs before actually performing retrieval */
        rr_vec.qid = query_vec.qid; 
        if (qrels_fd != UNDEF && 1 != seek_rr_vec (qrels_fd, &rr_vec))
            continue;

        if (query_vec.qid > global_end)
            break;
        if (UNDEF == add_new_query (&query_vec))
            return (UNDEF);
    }

    /* Get each document in turn, and compare the doc to each query
    while (1) { */
    for (did = start_did; did <= end_did; did++) {
        if (UNDEF == (status = get_doc.ptab->proc (&did, &vec,
                                                   get_doc.inst)))
            return (UNDEF);

        if (status == 0)
            continue;

        /* Check to see if tracing is desired. */
        SET_TRACE (vec.id_num.id);

        vec_pair.vec2 = &vec;
        /* Compare against each of the queries in qvec_list */
        for (i = 0; i < num_queries; i++) {
            vec_pair.vec1 = &query_list[i].vec;

            if (UNDEF == vec_vec.ptab->proc (&vec_pair, &sim, vec_vec.inst))
                return (UNDEF); 
            PRINT_TRACE (10, print_long, &vec_pair.vec1->id_num.id);
            PRINT_TRACE (10, print_long, &vec_pair.vec2->id_num.id);
            PRINT_TRACE (8, print_float, &sim);
            if (sim > 0.0 && sim >= query_list[i].result.min_top_sim) {
                (void) store_top (&sim, vec.id_num.id, 
                                  &query_list[i].result,
                                  &query_list[i].min_sim_index);
            }
        }
    }

    /* Store results for each query in qvec_list */
    for (i = 0; i < num_queries; i++) {
        /* Sort the top results by decreasing sim */
        qsort ((char *) query_list[i].result.top_results,
               (int) query_list[i].result.num_top_results,
               sizeof (TOP_RESULT),
               comp_top_sim);
        seendoc_vec.num_tr = 0;
        if (UNDEF == store_results.ptab->proc (&query_list[i].result,
                                               &seendoc_vec,
                                               store_results.inst))
            return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving retrieve_seq");
    return (status);
}

int
close_retrieve_seq (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_retrieve_seq");

    if (UNDEF == get_doc.ptab->close_proc(get_doc.inst))
        return (UNDEF);

    if (UNDEF == get_query.ptab->close_proc (get_query.inst) ||
        UNDEF == vec_vec.ptab->close_proc (vec_vec.inst) ||
        UNDEF == store_results.ptab->close_proc (store_results.inst)) {
        return (UNDEF);
    }

    if (qrels_fd != UNDEF && UNDEF == close_rr_vec (qrels_fd))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_retrieve_seq");
    return (0);
}

static int
add_new_query (query_vec)
QUERY_VECTOR *query_vec;
{
    if (num_queries >= max_query_list) {
        if (max_query_list == 0) {
            if (NULL == (query_list = (QUERY_LIST *)
                         malloc (QUERY_INCR * sizeof (QUERY_LIST))))
                return (UNDEF);
        }
        else {
            if (NULL == (query_list = (QUERY_LIST *)
                         realloc ((char *) query_list,
                                  (unsigned) (max_query_list + QUERY_INCR)
                                  * sizeof (QUERY_LIST))))
                return (UNDEF);
        }
        max_query_list += QUERY_INCR;
    }

    if (UNDEF == copy_vec (&query_list[num_queries].vec,
                           (VEC *) query_vec->vector))
        return (UNDEF);

    query_list[num_queries].result.num_full_results = 0;
    query_list[num_queries].result.full_results = NULL;
    query_list[num_queries].result.num_top_results = num_wanted;
    if (NULL == (query_list[num_queries].result.top_results = (TOP_RESULT *)
                 malloc ((unsigned) num_wanted * sizeof (TOP_RESULT))))
        return (UNDEF);
    (void) bzero ((char *) query_list[num_queries].result.top_results,
                  num_wanted * sizeof (TOP_RESULT));
    query_list[num_queries].result.min_top_sim = 0.0;
    query_list[num_queries].min_sim_index = 0;
    query_list[num_queries].result.qid = query_vec->qid;
    num_queries++;
    return (1);
}


/* store_top is same code as standard rank_tr except optimizes since we
   know this doc belongs in top_results */
static void
store_top (sim, did, results, min_sim_index)
float *sim;
long did;
RESULT *results;
long *min_sim_index;
{
    TOP_RESULT *end_res = &(results->top_results[results->num_top_results]);

    TOP_RESULT *min_res;   /* Position of doc with current min sim */
    TOP_RESULT *new_min_res; /* Position of doc with new min sim */
    TOP_RESULT *temp_res;
    TOP_RESULT *save_res;
    TOP_RESULT save_min;   /* Saved copy of min_res */

    min_res = &(results->top_results[*min_sim_index]);

    save_min.did = min_res->did;
    save_min.sim = min_res->sim;

    min_res->sim = *sim;
    min_res->did = did;

    new_min_res = results->top_results;
    save_res = (TOP_RESULT *) NULL;
    for (temp_res = results->top_results;
         temp_res < end_res;
         temp_res++) {
        if (did == temp_res->did && temp_res != min_res)
            save_res = temp_res;
        if (temp_res->sim < new_min_res->sim ||
            (temp_res->sim == new_min_res->sim &&
             temp_res->did < new_min_res->did)) {
            new_min_res = temp_res;
        }
    }

    if (save_res != (TOP_RESULT *) NULL) {
        /* New doc was already in top docs and now is represented twice */
        /* Restore old min_res in old location of new doc */
        /* Assumes new similarity for new doc is greater than old */
        /* similarity */
        save_res->sim = save_min.sim;
        save_res->did = save_min.did;
        if (save_res->sim < new_min_res->sim ||
            (save_res->sim == new_min_res->sim &&
             save_res->did < new_min_res->did)) {
            new_min_res = save_res;
        }
    }

    /* print_top_results (results, NULL); */
    results->min_top_sim = new_min_res->sim;
    *min_sim_index = new_min_res - results->top_results;
}

static int
comp_top_sim (ptr1, ptr2)
TOP_RESULT *ptr1;
TOP_RESULT *ptr2;
{
    if (ptr1->sim > ptr2->sim ||
        (ptr1->sim == ptr2->sim &&
         ptr1->did > ptr2->did))
        return (-1);
    return (1);
}
