#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/vec_vec_o.c,v 11.2 1993/02/03 16:49:40 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Copy a vector relational object
 *1 local.ocr.obj.vec_expvec
 *2 vec_expvec_obj (in_vec_file, out_vec_file, inst)
 *3   char *in_vec_file;
 *3   char *out_vec_file;
 *3   int inst;

 *4 init_vec_expvec_obj (spec, unused)
 *5   "convert.vec_expvec.doc_file"
 *5   "convert.vec_expvec.doc_file.rmode"
 *5   "convert.vec_expvec.doc_file.rwcmode"
 *5   "vec_expvec_obj.trace"
 *4 close_vec_expvec_obj(inst)

 *6 global_start and global_end are checked.  Only those vectors with
 *6 id_num falling between them are converted.
 *7 Read vector tuples from in_vec_file and copy to out_vec_file.
 *7 If in_vec_file is NULL then doc_file is used.
 *7 If out_vec_file is NULL, then error.
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
#include "io.h"

static char *default_vector_file;
static long read_mode;
static long write_mode;
static PROC_INST expand_func;


static SPEC_PARAM spec_args[] = {
    {"ocr.vec_expvec.query_file",  getspec_dbfile, (char *)&default_vector_file},
    {"ocr.vec_expvec.query_file.rmode",getspec_filemode, (char *) &read_mode},
    {"ocr.vec_expvec.query_file.rwcmode",getspec_filemode, (char *) &write_mode},
    {"ocr.vec_expvec.expand",     getspec_func, (char *) &expand_func.ptab},
    TRACE_PARAM ("vec_expvec_obj.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


int
init_vec_expvec_obj (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_vec_expvec_obj");
    if (UNDEF == (expand_func.inst = expand_func.ptab->init_proc (spec,NULL)))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_vec_expvec_obj");

    return (0);
}

int
vec_expvec_obj (in_vec_file, out_vec_file, inst)
char *in_vec_file;
char *out_vec_file;
int inst;
{
    VEC in_vector, out_vector;
    int status;
    int in_index, out_index;
    long did_index;

    PRINT_TRACE (2, print_string, "Trace: entering vec_expvec_obj");

    if (in_vec_file == NULL)
        in_vec_file = default_vector_file;
    if (UNDEF == (in_index = open_vector (in_vec_file, read_mode))){
        return (UNDEF);
    }

    if (! VALID_FILE (out_vec_file)) {
        set_error (SM_ILLPA_ERR, "vec_expvec_obj", out_vec_file);
        return (UNDEF);
    }
    if (UNDEF == (out_index = open_vector (out_vec_file, write_mode))){
        return (UNDEF);
    }

    /* Copy each vector in turn */
    in_vector.id_num.id = 0;
    EXT_NONE(in_vector.id_num.ext);
    if (global_start > 0) {
        in_vector.id_num.id = global_start;
        if (UNDEF == seek_vector (in_index, &in_vector)) {
            return (UNDEF);
        }
    }

    did_index = 0;
    while (1 == (status = read_vector (in_index, &in_vector)) &&
           in_vector.id_num.id <= global_end) {

        if (UNDEF == expand_func.ptab->proc (&in_vector,
                                             &out_vector,
                                             expand_func.inst))
            return (UNDEF);

        if (UNDEF == seek_vector (out_index, &out_vector) ||
            1 != write_vector (out_index, &out_vector))
            return (UNDEF);
    }

    if (UNDEF == close_vector (out_index) || 
        UNDEF == close_vector (in_index))
        return (UNDEF);
    
    PRINT_TRACE (2, print_string, "Trace: leaving print_vec_expvec_obj");
    return (status);
}

int
close_vec_expvec_obj(inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_vec_expvec_obj");

    PRINT_TRACE (2, print_string, "Trace: entering close_vec_expvec_obj");
    return (0);
}
