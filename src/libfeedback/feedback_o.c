#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfeedback/feedback_o.c,v 11.2 1993/02/03 16:50:04 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Top-level procedure to make feedback queries
 *1 feedback.feedback.feedback_obj
 *2 feedback_obj (unused1, unused2, inst)
 *3   char *unused1;
 *3   char *unused2;
 *3   int inst;
 *4 init_feedback_obj (spec, unused)
 *5   "feedback.get_query"
 *5   "feedback.get_seen_docs"
 *5   "feedback.feedback"
 *5   "feedback.trace"
 *4 close_feedback_obj (inst)
 *6 global_start, global_end matched against query_id to determine if
 *6 this query should be run.
 *7 Steps are:
 *7    get a query
 *7    construct new feedback query based on original query and
 *7           previously seen docs (returned by get_seen_docs)
 *7    store feedback query
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
#include "feedback.h"


static PROC_INST get_query,
    get_seen_docs,
    pfeedback;
static char *feedback_file;
static long feedback_rwcmode;
static long feedback_rwmode;

static SPEC_PARAM spec_args[] = {
    {"feedback.get_query",        getspec_func, (char *) &get_query.ptab},
    {"feedback.get_seen_docs",    getspec_func, (char *) &get_seen_docs.ptab},
    {"feedback.feedback_file",    getspec_dbfile, (char *) &feedback_file},
    {"feedback.feedback_file.rwcmode",getspec_filemode, (char *) &feedback_rwcmode},
    {"feedback.feedback_file.rwmode",getspec_filemode, (char *) &feedback_rwmode},
    {"feedback.feedback",         getspec_func, (char *) &pfeedback.ptab},
    TRACE_PARAM ("feedback.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);


int
init_feedback_obj (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_feedback_obj");

    /* Call all initialization procedures */
    if (UNDEF == (get_query.inst =
                  get_query.ptab->init_proc (spec, NULL)) ||
        UNDEF == (get_seen_docs.inst =
                  get_seen_docs.ptab->init_proc (spec, NULL)) ||
        UNDEF == (pfeedback.inst = pfeedback.ptab->init_proc (spec, NULL))) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving init_feedback_obj");
    return (0);
}

int
feedback_obj (unused1, unused2, inst)
char *unused1;
char *unused2;
int inst;
{
    int status;
    QUERY_VECTOR query_vec, fdbk_query;
    RETRIEVAL retrieval;
    TR_VEC tr_vec;
    int feedback_fd;

    PRINT_TRACE (2, print_string, "Trace: entering feedback_obj");

    if (UNDEF == (feedback_fd = open_vector (feedback_file, feedback_rwmode))){
        clr_err();
        if (UNDEF == (feedback_fd = open_vector (feedback_file,
                                                 feedback_rwcmode)))
            return (UNDEF);
    }

    retrieval.query = &query_vec;
    retrieval.tr = &tr_vec;
    fdbk_query.vector = NULL;

    while (1 == (status = get_query.ptab->proc (NULL,
                                                &query_vec,
                                                get_query.inst)))  {
        SET_TRACE (query_vec.qid);

        if (query_vec.qid < global_start)
            continue;
        if (query_vec.qid > global_end)
            break;
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

        if (UNDEF == seek_vector (feedback_fd, (VEC *) fdbk_query.vector) ||
            UNDEF == write_vector (feedback_fd, (VEC *) fdbk_query.vector))
            return (UNDEF);
    }

    if (UNDEF == close_vector (feedback_fd))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving feedback_obj");
    return (status);
}

int
close_feedback_obj (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_feedback_obj");

    if (UNDEF == get_query.ptab->close_proc (get_query.inst) ||
        UNDEF == get_seen_docs.ptab->close_proc (get_seen_docs.inst) ||
        UNDEF == pfeedback.ptab->close_proc (pfeedback.inst)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_feedback_obj");
    return (0);
}


