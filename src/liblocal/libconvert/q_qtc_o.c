#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/q_qtc_o.c,v 11.2 1993/02/03 16:49:17 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Take a vector query relational object, and add term clusters to it
 *1 local.convert.obj.q_qtc_obj
 *2 q_qtc_obj (in_query_file, out_query_file, inst)
 *3   char *in_query_file;
 *3   char *out_query_file;
 *3   int inst;

 *4 init_q_qtc_obj (spec, unused)
 *5   "convert.q_qtc.query_file"
 *5   "convert.q_qtc.query_file.rmode"
 *5   "convert.q_qtc.query_file.rwmode"
 *5   "q_qtc_obj.trace"
 *4 close_q_qtc_obj(inst)

 *6 global_start and global_end are checked.  Only those vectors with
 *6 id_num falling between them are converted.
 *7 Read vector tuples from in_query file and add clusters to them using the
 *7 procedure "q_qtc".
 *7 If in_query_file is NULL then query_file is used.
 *7 If out_query_file is NULL, then query is used (overwritten)
 *7 Return UNDEF if error, else 0;
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "context.h"
#include "proc.h"
#include "vector.h"
#include "inv.h"
#include "io.h"

static char *default_query_file;
static long query_rmode;
static long query_rwmode;


static SPEC_PARAM spec_args[] = {
    {"convert.q_qtc.query_file", getspec_dbfile, (char *) &default_query_file},
    {"convert.q_qtc.query_file.rmode",getspec_filemode, (char *) &query_rmode},
    {"convert.q_qtc.query_file.rwmode",getspec_filemode, (char *) &query_rwmode},
    TRACE_PARAM ("q_qtc_obj.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


static int in_query_fd;
static int out_query_fd;
static int q_qtc_inst;

int init_q_qtc(), q_qtc(), close_q_qtc();

int
init_q_qtc_obj (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_q_qtc_obj");

    if (UNDEF == (q_qtc_inst = init_q_qtc (spec, (char *) NULL)))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_q_qtc_obj");

    return (0);
}

int
q_qtc_obj (in_query_file, out_query_file, inst)
char *in_query_file;
char *out_query_file;
int inst;
{
    VEC in_query, out_query;
    int status;

    PRINT_TRACE (2, print_string, "Trace: entering q_qtc_obj");

    if (in_query_file == NULL)
        in_query_file = default_query_file;
    if (UNDEF == (in_query_fd = open_vector (in_query_file, query_rmode))){
        return (UNDEF);
    }

    if (out_query_file == NULL)
	out_query_file = default_query_file;
    if (UNDEF == (out_query_fd = open_vector (out_query_file, query_rwmode))){
	clr_err();
	if (UNDEF == (out_query_fd = open_vector (out_query_file, SCREATE | query_rwmode))){
	    return (UNDEF);
	}
    }

    /* Convert each vector in turn */
    in_query.id_num.id = 0;
    EXT_NONE(in_query.id_num.ext);
    if (global_start > 0) {
        in_query.id_num.id = global_start;
        if (UNDEF == seek_vector (in_query_fd, &in_query)) {
            return (UNDEF);
        }
    }

    while (1 == (status = read_vector (in_query_fd, &in_query)) &&
           in_query.id_num.id <= global_end) {
	SET_TRACE (in_query.id_num.id);
        if (1 != q_qtc (&in_query, &out_query, q_qtc_inst)) {
            return (UNDEF);
        }
	if (UNDEF == seek_vector (out_query_fd, &out_query) ||
	    UNDEF == write_vector (out_query_fd, &out_query))
	    return (UNDEF);
    }

    if (UNDEF == close_vector (in_query_fd))
        return (UNDEF);

    if (UNDEF == close_vector (out_query_fd))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving print_q_qtc_obj");
    return (status);
}

int
close_q_qtc_obj(inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_q_qtc_obj");

    if (UNDEF == close_q_qtc (q_qtc_inst))
        return (UNDEF);


    PRINT_TRACE (2, print_string, "Trace: entering close_q_qtc_obj");
    return (0);
}
