#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libretrieve/ret_fdbk.c,v 11.2 1993/02/03 16:54:38 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Top-level procedure to make and run feedback queries retrieving docs
 *1 retrieve.top.ret_fdbk
 *2 ret_fdbk (unused1, unused2, inst)
 *3   char *unused1;
 *3   char *unused2;
 *3   int inst;
 *4 init_ret_fdbk (spec, unused)
 *5   "retrieve.get_query"
 *5   "retrieve.get_seen_docs"
 *5   "retrieve.coll_sim"
 *5   "retrieve.output";
 *5   "retrieve.feedback"
 *5   "retrieve.trace"
 *4 close_ret_fdbk (inst)
 *6 global_start, global_end matched against query_id to determine if
 *6 this query should be run.
 *7 Top-level procedure to run a feedback retrieval with the specified indexed
 *7 query collection on the specified indexed document collection.
 *7 Steps are:
 *7    get a query
 *7    foreach query
 *7       construct new feedback query based on original query and
 *7           previously seen docs (returned by get_seen_docs)
 *7       submit new query to retrieval method.
 *7       store final results.
 *9 Need to get feedback evaluation better hooked in
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


static PROC_INST get_query,
    get_seen_docs,
    coll_sim,
    store_results,
    pfeedback;

static SPEC_PARAM spec_args[] = {
    {"retrieve.get_query",        getspec_func, (char *) &get_query.ptab},
    {"retrieve.get_seen_docs",    getspec_func, (char *) &get_seen_docs.ptab},
    {"retrieve.coll_sim",         getspec_func, (char *) &coll_sim.ptab},
    {"retrieve.output",           getspec_func, (char *) &store_results.ptab},
    {"retrieve.feedback",         getspec_func, (char *) &pfeedback.ptab},
    TRACE_PARAM ("retrieve.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);


int
init_ret_fdbk (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_ret_fdbk");

    /* Call all initialization procedures */
    if (UNDEF == (get_query.inst =
                  get_query.ptab->init_proc (spec, NULL)) ||
        UNDEF == (get_seen_docs.inst =
                  get_seen_docs.ptab->init_proc (spec, NULL)) ||
        UNDEF == (coll_sim.inst =
                  coll_sim.ptab->init_proc (spec, NULL)) ||
        UNDEF == (store_results.inst =
                  store_results.ptab->init_proc (spec, NULL)) ||
        UNDEF == (pfeedback.inst = pfeedback.ptab->init_proc (spec, NULL))) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving init_ret_fdbk");
    return (0);
}

int
ret_fdbk (unused1, unused2, inst)
char *unused1;
char *unused2;
int inst;
{
    int status;
    QUERY_VECTOR query_vec, fdbk_query;
    RESULT results;
    RETRIEVAL retrieval;
    TR_VEC tr_vec;

    PRINT_TRACE (2, print_string, "Trace: entering ret_fdbk");

    retrieval.query = &query_vec;
    retrieval.tr = &tr_vec;
    fdbk_query.vector = NULL;

    while (1 == (status = get_query.ptab->proc (NULL,
                                                &query_vec,
                                                get_query.inst)))  {
        if (query_vec.qid < global_start)
            continue;
        if (query_vec.qid > global_end)
            break;
        results.qid = query_vec.qid;
        tr_vec.num_tr = 0;

        if (UNDEF == get_seen_docs.ptab->proc (&query_vec,
                                               &tr_vec,
                                               get_seen_docs.inst))
            return (UNDEF);

        /* NOTE TO ME: make sure fdbk copies query (could be same
           address as fdbk_query) */
        if (UNDEF == pfeedback.ptab->proc (&retrieval,
                                           &fdbk_query,
                                           pfeedback.inst))
            return (UNDEF);
        if (fdbk_query.vector != NULL)
            query_vec.vector = fdbk_query.vector;
        if (UNDEF == coll_sim.ptab->proc (&retrieval, &results, coll_sim.inst))
            return (UNDEF);
        if (UNDEF == store_results.ptab->proc (&results,
                                               &tr_vec,
                                               store_results.inst))
            return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving ret_fdbk");
    return (status);
}

int
close_ret_fdbk (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_ret_fdbk");

    if (UNDEF == get_query.ptab->close_proc (get_query.inst) ||
        UNDEF == get_seen_docs.ptab->close_proc (get_seen_docs.inst) ||
        UNDEF == coll_sim.ptab->close_proc (coll_sim.inst) ||
        UNDEF == store_results.ptab->close_proc (store_results.inst) ||
        UNDEF == pfeedback.ptab->close_proc (pfeedback.inst)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_ret_fdbk");
    return (0);
}


