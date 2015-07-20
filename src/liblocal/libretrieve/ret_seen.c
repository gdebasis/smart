#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libretrieve/retrieve.c,v 11.2 1993/02/03 16:54:27 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Top-level procedure to run query collection against seen docs only
 *1 local.retrieve.top.retrieve_seen
 *2 retrieve_seen (unused1, unused2, inst)
 *3   char *unused1;
 *3   char *unused2;
 *3   int inst;
 *4 init_retrieve_seen (spec, unused)
 *5   "retrieve.seen_docs_file"
 *5   "retrieve.seen_docs_file.rmode"
 *5   "retrieve.tr_file"
 *5   "retrieve.tr_file.rmode"
 *5   "retrieve.q_trvec"
 *5   "retrieve.trace"
 *4 close_retrieve_seen (inst)
 *6 global_start, global_end matched against query_id to determine if
 *6 this query should be run.

 *7 Top-level procedure to re-run a previously run retrieval.  Each
 *7 query's tr_vec is given to q_trvec which returns a new tr_vec
 *7 to be output. q_trvec is responsible for getting the query and
 *7 documents in the form that it needs.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "tr_vec.h"
#include "retrieve.h"

static PROC_INST q_trvec;        /* proc to run query against all
                                    docs in tr_vec */
static char *seen_docs_file;
static long seen_docs_mode;

static SPEC_PARAM spec_args[] = {
    {"retrieve.q_tr",          getspec_func, (char *) &q_trvec.ptab},
    {"retrieve.seen_docs_file",       getspec_dbfile,(char *) &seen_docs_file},
    {"retrieve.seen_docs_file.rmode",getspec_filemode,(char *)&seen_docs_mode},
    TRACE_PARAM ("retrieve.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);


static int seen_fd;
static int ret_tr_inst;

int
init_retrieve_seen (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_retrieve_seen");

    /* Call all initialization procedures */
    if (UNDEF == (q_trvec.inst = q_trvec.ptab->init_proc (spec, NULL)))
        return (UNDEF);

    if (UNDEF == (ret_tr_inst = init_ret_tr (spec, (char *) NULL)))
        return (UNDEF);

    /* Open input file */
    if ( ! VALID_FILE (seen_docs_file)) {
        set_error (SM_ILLPA_ERR, "retrieve_seen", "retrieve.seen_docs_file");
        return (UNDEF);
    }
    if (UNDEF == (seen_fd = open_tr_vec (seen_docs_file, seen_docs_mode)))
            return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_retrieve_seen");
    return (0);
}

int
retrieve_seen (unused1, unused2, inst)
char *unused1;
char *unused2;
int inst;
{
    int status;
    TR_VEC seen_vec;
    RESULT results;
    TR_VEC tr_vec;

    PRINT_TRACE (2, print_string, "Trace: entering retrieve_seen");

    /* Get each tr_vec, call q_trvec, and write out results */
    while (1 == (status = read_tr_vec (seen_fd, &seen_vec))) {
        if (seen_vec.qid < global_start)
            continue;
        if (seen_vec.qid > global_end)
            break;
        SET_TRACE (seen_vec.qid);
        if (UNDEF == q_trvec.ptab->proc (&seen_vec, &results, q_trvec.inst))
            return (UNDEF);

        tr_vec.num_tr = 0;
        if (UNDEF == ret_tr (&results, &tr_vec, ret_tr_inst))
            return (UNDEF);

    }

    PRINT_TRACE (2, print_string, "Trace: leaving retrieve_seen");
    return (status);
}

int
close_retrieve_seen (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_retrieve_seen");

    if (UNDEF == q_trvec.ptab->close_proc(q_trvec.inst))
        return (UNDEF);

    if (UNDEF == close_ret_tr (ret_tr_inst))
        return (UNDEF);

    if (UNDEF == close_tr_vec (seen_fd))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_retrieve_seen");
    return (0);
}
