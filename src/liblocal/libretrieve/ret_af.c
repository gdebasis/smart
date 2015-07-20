#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libretrieve/ret_twopass.c,v 11.2 1993/02/03 16:54:38 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Top-level retrieval feedback expand after first pass and run again
 *1 local.retrieve.top.ret_af
 *2 ret_af (unused1, unused2, inst)
 *3   char *unused1;
 *3   char *unused2;
 *3   int inst;
 *4 init_ret_af (spec, unused)
 *5   "retrieve.get_query"
 *5   "retrieve.coll_sim"
 *5   "retrieve.output";
 *5   "retrieve.feedback"
 *5   "retrieve.trace"
 *4 close_ret_af (inst)
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
#include "inst.h"
#include "rr_vec.h"
#include "tr_vec.h"

static PROC_TAB *get_query_ptab,
    *get_seen_docs_ptab,
    *coll_sim_ptab,
    *store_results_ptab,
    *pfeedback_ptab;
static char *qrels_file;
static long qrels_mode;

static SPEC_PARAM spec_args[] = {
    {"retrieve.get_query",        getspec_func, (char *) &get_query_ptab},
    {"retrieve.get_seen_docs",    getspec_func, (char *) &get_seen_docs_ptab},
    {"retrieve.coll_sim",         getspec_func, (char *) &coll_sim_ptab},
    {"retrieve.output",           getspec_func, (char *) &store_results_ptab},
    {"retrieve.feedback",         getspec_func, (char *) &pfeedback_ptab},
    {"retrieve.qrels_file",       getspec_dbfile,(char *) &qrels_file},
    {"retrieve.qrels_file.rmode", getspec_filemode,(char *) &qrels_mode},
    TRACE_PARAM ("retrieve.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

typedef struct {
    int valid_info;     /* bookkeeping */

    PROC_INST get_query;
    PROC_INST get_seen_docs;
    PROC_INST coll_sim_pass1;
    PROC_INST coll_sim_pass2;
    PROC_INST store_results;
    PROC_INST pfeedback;

    int qrels_fd;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;


int
init_ret_af (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_ret_af");

    NEW_INST( new_inst );
    if (UNDEF == new_inst)
        return UNDEF;
    ip = &info[new_inst];
    ip->valid_info = 1;

    /* Call all initialization procedures */
    ip->get_query.ptab = get_query_ptab;
    ip->get_seen_docs.ptab = get_seen_docs_ptab;
    ip->coll_sim_pass1.ptab = coll_sim_ptab;
    ip->coll_sim_pass2.ptab = coll_sim_ptab;
    ip->store_results.ptab = store_results_ptab;
    ip->pfeedback.ptab = pfeedback_ptab;
    if (UNDEF == (ip->get_query.inst =
                  get_query_ptab->init_proc (spec, NULL)) ||
        UNDEF == (ip->get_seen_docs.inst =
                  get_seen_docs_ptab->init_proc (spec, NULL)) ||
        UNDEF == (ip->coll_sim_pass1.inst =
                  coll_sim_ptab->init_proc (spec, "pass1.")) ||
        UNDEF == (ip->coll_sim_pass2.inst =
                  coll_sim_ptab->init_proc (spec, "pass2.")) ||
        UNDEF == (ip->store_results.inst =
                  store_results_ptab->init_proc (spec, NULL)) ||
        UNDEF == (ip->pfeedback.inst = 
                  pfeedback_ptab->init_proc (spec, NULL))) {
        return (UNDEF);
    }

    ip->qrels_fd = UNDEF;
    if (VALID_FILE (qrels_file)) {
        if (UNDEF == (ip->qrels_fd = open_rr_vec (qrels_file, qrels_mode)))
            return (UNDEF);
    }

    ip->valid_info++;
    PRINT_TRACE (2, print_string, "Trace: leaving init_ret_af");
    return (new_inst);
}

int
ret_af (unused1, unused2, inst)
char *unused1;
char *unused2;
int inst;
{
    STATIC_INFO *ip;
    int status;
    QUERY_VECTOR query_vec, fdbk_query;
    RESULT results;
    RETRIEVAL retrieval, fdbk_retrieval;
    TR_VEC tr_vec, pass1_tr_vec;
    RR_VEC rr_vec;
    long i;

    PRINT_TRACE (2, print_string, "Trace: entering ret_af");
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "ret_af");
        return (UNDEF);
    }
    ip  = &info[inst];


    retrieval.query = &query_vec;
    retrieval.tr = &tr_vec;
    fdbk_query.vector = NULL;
    fdbk_retrieval.query = &query_vec;
    fdbk_retrieval.tr = &pass1_tr_vec;


    while (1 == (status = ip->get_query.ptab->proc (NULL,
                                                &query_vec,
                                                ip->get_query.inst)))  {
        if (query_vec.qid < global_start)
            continue;
        if (query_vec.qid > global_end)
            break;
        /* Check to see if any rel docs before actually performing retrieval */
        rr_vec.qid = query_vec.qid; 
        if (ip->qrels_fd != UNDEF && 1 != seek_rr_vec (ip->qrels_fd, &rr_vec))
            continue;

        SET_TRACE (query_vec.qid);

        results.qid = query_vec.qid;
        tr_vec.num_tr = 0;

        if (UNDEF == ip->get_seen_docs.ptab->proc (&query_vec,
                                                   &tr_vec,
                                                   ip->get_seen_docs.inst))
            return (UNDEF);
        
        /* Perform first pass of retrieval */
        /* Change to pass in num_wanted */
        if (UNDEF == ip->coll_sim_pass1.ptab->proc (&retrieval,
                                                    &results,
                                                    ip->coll_sim_pass1.inst))
            return (UNDEF);

        /* Mark top docs as relevant */
        pass1_tr_vec.num_tr = 0;
        if (UNDEF == res_tr (results, &pass1_tr_vec, 0))
            return (UNDEF);
        for (i = 0; i < pass1_tr_vec.num_tr; i++) {
            pass1_tr_vec.tr[i].rel = 1;
        }

        /* NOTE TO ME: make sure fdbk copies query (could be same
           address as fdbk_query) */
        if (UNDEF == ip->pfeedback.ptab->proc (&fdbk_retrieval,
                                               &fdbk_query,
                                               ip->pfeedback.inst))
            return (UNDEF);
        if (fdbk_query.vector != NULL)
            query_vec.vector = fdbk_query.vector;
        if (UNDEF == ip->coll_sim_pass2.ptab->proc (&retrieval,
                                                    &results,
                                                    ip->coll_sim_pass2.inst))
            return(UNDEF);
        if (UNDEF == ip->store_results.ptab->proc (&results,
                                                   &tr_vec,
                                                   ip->store_results.inst))
            return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving ret_af");
    return (status);
}

int
close_ret_af (inst)
int inst;
{
    STATIC_INFO *ip;
    PRINT_TRACE (2, print_string, "Trace: entering close_ret_af");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_ret_af");
        return (UNDEF);
    }
    ip  = &info[inst];
    if (UNDEF == ip->get_query.ptab->close_proc (ip->get_query.inst) ||
        UNDEF == ip->get_seen_docs.ptab->close_proc (ip->get_seen_docs.inst) ||
        UNDEF == ip->coll_sim_pass1.ptab->close_proc (ip->coll_sim_pass1.inst) ||
        UNDEF == ip->coll_sim_pass2.ptab->close_proc (ip->coll_sim_pass2.inst) ||
        UNDEF == ip->store_results.ptab->close_proc (ip->store_results.inst) ||
        UNDEF == ip->pfeedback.ptab->close_proc (ip->pfeedback.inst)) {
        return (UNDEF);
    }
    if (ip->qrels_fd != UNDEF && UNDEF == close_rr_vec (ip->qrels_fd))
        return (UNDEF);

    ip->valid_info = 0;
    PRINT_TRACE (2, print_string, "Trace: leaving close_ret_af");
    return (0);
}


