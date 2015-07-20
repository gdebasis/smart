#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libretrieve/retrieve_all.c,v 11.2 1993/02/03 16:54:42 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Top-level procedure to run query collection retrieving docs
 *1 retrieve.top.retrieve_all
 *2 retrieve_all (unused1, unused2, inst)
 *3   char *unused1;
 *3   char *unused2;
 *3   int inst;
 *4 init_retrieve_all (spec, unused)
 *5   "retrieve.get_query"
 *5   "retrieve.get_seen_docs"
 *5   "retrieve.coll_sim"
 *5   "retrieve.output";
 *5   "retrieve.trace"
 *4 close_retrieve_all (inst)
 *6 global_start, global_end matched against query_id to determine if
 *6 this query should be run.

 *7 Top-level procedure to run retrievals with the specified indexed
 *7 query collection on the specified indexed document collection.
 *7 Foreach query returned by get_query, get all previously seen docs
 *7 for this query (it might be a feedback query where the user will not
 *7 want to see docs retrieved earlier), and then submit
 *7 query to specified retrieval method coll_sim, getting back RESULTS to
 *7 be stored using retrieve.output.
 *7 This differs from top.retrieve in that all queries are run, instead
 *7 of just those with relevant documents.
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

/* Single procedure to run a retrieval with the specified indexed
   query collection on the specified indexed document collection

   Steps are:
   get a query
   foreach query
      submit query to specified retrieval method, getting back RESULTS.
*/


static PROC_INST get_query,
    get_seen_docs,
    coll_sim,
    store_results;

static SPEC_PARAM spec_args[] = {
    {"retrieve.get_query",        getspec_func, (char *) &get_query.ptab},
    {"retrieve.get_seen_docs",    getspec_func, (char *) &get_seen_docs.ptab},
    {"retrieve.coll_sim",         getspec_func, (char *) &coll_sim.ptab},
    {"retrieve.output",           getspec_func, (char *) &store_results.ptab},
    TRACE_PARAM ("retrieve.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

int
init_retrieve_all (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_retrieve_all");

    /* Call all initialization procedures */
    if (UNDEF == (get_query.inst =
                  get_query.ptab->init_proc (spec, NULL)) ||
        UNDEF == (get_seen_docs.inst =
                  get_seen_docs.ptab->init_proc (spec, NULL)) ||
        UNDEF == (coll_sim.inst =
                  coll_sim.ptab->init_proc (spec, NULL)) ||
        UNDEF == (store_results.inst =
                  store_results.ptab->init_proc (spec, NULL))) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving init_retrieve_all");
    return (0);
}

int
retrieve_all (unused1, unused2, inst)
char *unused1;
char *unused2;
int inst;
{
    int status;
    QUERY_VECTOR query_vec;
    RESULT results;
    RETRIEVAL retrieval;
    TR_VEC tr_vec;

    PRINT_TRACE (2, print_string, "Trace: entering retrieve_all");

    retrieval.query = &query_vec;
    retrieval.tr = &tr_vec;

    while (1 == (status = get_query.ptab->proc (NULL,
                                                &query_vec,
                                                get_query.inst)))  {
        if (query_vec.qid < global_start)
            continue;
        if (query_vec.qid > global_end)
            break;
        SET_TRACE (query_vec.qid);

        results.qid = query_vec.qid;
        tr_vec.num_tr = 0;

        if (UNDEF == get_seen_docs.ptab->proc (&query_vec,
                                               &tr_vec,
                                               get_seen_docs.inst))
            return (UNDEF);
        if (UNDEF == coll_sim.ptab->proc (&retrieval, &results, coll_sim.inst))
            return (UNDEF);
        if (UNDEF == store_results.ptab->proc (&results,
                                               &tr_vec,
                                               store_results.inst))
            return (UNDEF);
    }
    PRINT_TRACE (2, print_string, "Trace: leaving retrieve_all");
    return (status);
}

int
close_retrieve_all (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_retrieve_all");

    if (UNDEF == get_query.ptab->close_proc (get_query.inst) ||
        UNDEF == get_seen_docs.ptab->close_proc (get_seen_docs.inst) ||
        UNDEF == coll_sim.ptab->close_proc (coll_sim.inst) ||
        UNDEF == store_results.ptab->close_proc (store_results.inst)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_retrieve_all");
    return (0);
}


