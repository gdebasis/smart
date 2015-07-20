#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/tr_rel_o.c,v 11.2 1993/02/03 16:49:31 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Take a tr file, add relevance judgements, delete queries with no rel docs
 *1 convert.obj.tr_rel_obj
 *2 tr_rel_obj (in_tr_file, out_tr_file, inst)
 *3 char *in_tr_file;
 *3 char *out_tr_file;
 *3 int inst;
 *4 init_tr_rel_obj (spec, unused)
 *5   "tr_rel_obj.qrels_file"
 *5   "tr_rel_obj.tr_file.rwcmode"
 *5   "tr_rel_obj.trace"
 *4 close_tr_rel_obj (inst)

 *7 Return UNDEF if error, else 1;
***********************************************************************/

#include "common.h"
#include "param.h"
#include "io.h"
#include "functions.h"
#include "spec.h"
#include "trace.h"
#include "smart_error.h"
#include "rr_vec.h"
#include "tr_vec.h"

static char *qrels_file;
static long qrels_mode;
static long tr_rmode;
static long tr_rwcmode;

static SPEC_PARAM spec_args[] = {
    {"tr_rel_obj.qrels_file",        getspec_dbfile, (char *) &qrels_file},
    {"tr_rel_obj.qrels_file.rmode",  getspec_filemode, (char *) &qrels_mode},
    {"tr_rel_obj.tr_file.rmode",     getspec_filemode, (char *) &tr_rmode},
    {"tr_rel_obj.tr_file.rwcmode",   getspec_filemode, (char *) &tr_rwcmode},
    TRACE_PARAM ("tr_rel_obj.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int qrels_fd;

int
init_tr_rel_obj (spec, unused)
SPEC *spec;
char *unused;
{
    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_tr_rel_obj");

    if (! VALID_FILE (qrels_file) ||
        UNDEF == (qrels_fd = open_rr_vec (qrels_file, qrels_mode)))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_tr_rel_obj");

    return (0);
}

int
tr_rel_obj (in_tr_file, out_tr_file, inst)
char *in_tr_file;
char *out_tr_file;
int inst;
{
    RR_VEC qrels_vec;
    TR_VEC tr_vec;
    int in_tr_fd;
    int out_tr_fd;
    int tr_index, qrels_index;

    PRINT_TRACE (2, print_string, "Trace: entering tr_rel_obj");

    if (UNDEF == (in_tr_fd = open_tr_vec (in_tr_file, tr_rmode))) {
        return (UNDEF);
    }
    if (UNDEF == (out_tr_fd = open_tr_vec (out_tr_file, tr_rwcmode))) {
        return (UNDEF);
    }

    while (1 == read_tr_vec (in_tr_fd, &tr_vec)) {
        qrels_vec.qid = tr_vec.qid;
        if (1 != seek_rr_vec (qrels_fd, &qrels_vec))
            continue;
        if (1 != read_rr_vec (qrels_fd, &qrels_vec))
            return (UNDEF);
        /* add relevance judgments */
        for (tr_index = 0, qrels_index = 0;
             tr_index < tr_vec.num_tr && qrels_index < qrels_vec.num_rr;
             ) {
            if (tr_vec.tr[tr_index].did < qrels_vec.rr[qrels_index].did) {
                tr_vec.tr[tr_index].rel = 0;
                tr_index++;
            }
            else if (tr_vec.tr[tr_index].did > qrels_vec.rr[qrels_index].did)
                qrels_index++;
            else {
                tr_vec.tr[tr_index].rel = 1;
                tr_index++;
                qrels_index++;
            }
        }
        while (tr_index < tr_vec.num_tr) {
            tr_vec.tr[tr_index].rel = 0;
            tr_index++;
        }
        if (UNDEF == seek_tr_vec (out_tr_fd, &tr_vec) ||
            1 != write_tr_vec (out_tr_fd, &tr_vec))
            return (UNDEF);
    }

    if (UNDEF == close_tr_vec (in_tr_fd) ||
        UNDEF == close_tr_vec (out_tr_fd))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving tr_rel_obj");
    return (1);
}

int
close_tr_rel_obj (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_tr_rel_obj");

    if (UNDEF == close_rr_vec (qrels_fd))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_tr_rel_obj");
    
    return (0);
}

