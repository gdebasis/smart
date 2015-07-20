#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libretrieve/ret_tr.c,v 11.2 1993/02/03 16:54:29 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Store query retrieval results in top-rank file
 *1 retrieve.output.ret_tr
 *2 ret_tr (results, tr_vec, inst)
 *3   RESULT *results;
 *3   TR_VEC *tr_vec;
 *3   int inst;
 *4 init_ret_tr (spec, unused)
 *5   "retrieve.tr_file"
 *5   "retrieve.tr_file.rwmode"
 *5   "retrieve.qrels_file"
 *5   "retrieve.qrels_file.rmode"
 *5   "retrieve.output.trace"
 *4 close_ret_tr (inst)
 *7 Add results of a single query to tr_vec. tr_vec upon input has info 
 *7 from any previous iteration of this query.  If tr_file is a valid file
 *7 name, then also write out the top results to tr_file.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "io.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "context.h"
#include "retrieve.h"
#include "rr_vec.h"
#include "tr_vec.h"

static char *tr_file;
static long tr_file_mode;
static char *qrels_file;
static long qrels_file_mode;
static char store_nsim;

static SPEC_PARAM spec_args[] = {
    {"retrieve.tr_file",          getspec_dbfile, (char *) &tr_file},
    {"retrieve.tr_file.rwmode",   getspec_filemode, (char *) &tr_file_mode},
    {"retrieve.qrels_file",       getspec_dbfile, (char *) &qrels_file},
    {"retrieve.qrels_file.rmode", getspec_filemode, (char *) &qrels_file_mode},
    {"retrieve.store_nsim", getspec_bool, (char *) &store_nsim},	
    TRACE_PARAM ("retrieve.output.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static int tr_fd = UNDEF;
static int qrels_fd = UNDEF;

static RR_VEC qrels;


int
init_ret_tr (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_ret_tr");

    /* Open tr file, if desired */
    if (VALID_FILE (tr_file)) {
        if (UNDEF == (tr_fd = open_tr_vec (tr_file, tr_file_mode))) {
            clr_err();
            if (UNDEF == (tr_fd = open_tr_vec (tr_file, tr_file_mode|SCREATE)))
                return (UNDEF);
        }
    }

    /* Open qrels file, if desired */
    if (VALID_FILE (qrels_file) &&
        UNDEF == (qrels_fd = open_rr_vec (qrels_file, qrels_file_mode)))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_ret_tr");
    return (0);
}


/* Add results in proper format to tr_vec */
/* tr_vec upon input has info from any previous iteration of this query */
/* Add top results to tr_file if designated */
int
ret_tr (results, tr_vec, inst)
RESULT *results;
TR_VEC *tr_vec;
int inst;
{
    register RR_TUP *qr_ptr, *end_qr_ptr;
    long i;
    int status;

    PRINT_TRACE (2, print_string, "Trace: entering ret_tr");

    if (results == (RESULT *) NULL || tr_vec == (TR_VEC *) NULL)
        return (UNDEF);

    tr_vec->qid = results->qid;

    /* Add top_results to tr_vec.  Possibly add relevance info and write out */
    if (UNDEF == res_tr (results, tr_vec, store_nsim))
        return (UNDEF);
    if (qrels_fd != UNDEF) {
        qrels.qid = results->qid;
        if (1 == (status = seek_rr_vec (qrels_fd, &qrels)) &&
            1 == (status = read_rr_vec (qrels_fd, &qrels))) {
            qr_ptr = qrels.rr;
            end_qr_ptr = &qrels.rr[qrels.num_rr];
            for (i = 0; i < tr_vec->num_tr; i++) {
                tr_vec->tr[i].rel = 0;
                while (qr_ptr < end_qr_ptr && tr_vec->tr[i].did >qr_ptr->did)
                    qr_ptr++;
                if (qr_ptr < end_qr_ptr && tr_vec->tr[i].did == qr_ptr->did){
                    qr_ptr++;
                    tr_vec->tr[i].rel = 1;
                }
            }
        }
        else if (status == UNDEF)
            return (UNDEF);
    }

    /* output tr_vec to tr_file if desired */
    if (UNDEF != tr_fd && tr_vec->num_tr > 0) {
        if (UNDEF == seek_tr_vec (tr_fd, tr_vec) ||
            UNDEF == write_tr_vec (tr_fd, tr_vec)) 
            return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving ret_tr");
    return (1);
}


int
close_ret_tr (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_ret_tr");

    if (UNDEF != tr_fd && UNDEF == close_tr_vec (tr_fd))
        return (UNDEF);
    if (UNDEF != qrels_fd && UNDEF == close_rr_vec (qrels_fd))
        return (UNDEF);

    tr_fd = qrels_fd = UNDEF;

    PRINT_TRACE (2, print_string, "Trace: leaving close_ret_tr");
    return (0);
}
