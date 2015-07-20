#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/vecwt_vec_o.c,v 11.2 1993/02/03 16:49:40 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Copy a reweighted vector relational object
 *1 convert.obj.vecwt_vec_obj
 *2 vecwt_vec_obj (in_vec_file, out_vec_file, inst)
 *3   char *in_vec_file;
 *3   char *out_vec_file;
 *3   int inst;

 *4 init_vecwt_vec_obj (spec, unused)
 *5   "convert.vecwt_vec.doc_file"
 *5   "convert.vecwt_vec.doc_file.rmode"
 *5   "convert.doc.weight"
 *5   "convert.vecwt_vec.doc_file.rwcmode"
 *5   "vecwt_vec_obj.trace"
 *4 close_vecwt_vec_obj(inst)

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
static PROC_INST doc_weight;

static SPEC_PARAM spec_args[] = {
    {"convert.vecwt_vec.doc_file",getspec_dbfile,(char *) &default_vector_file},
    {"convert.vecwt_vec.doc_file.rmode",getspec_filemode,(char *) &read_mode},
    {"convert.vecwt_vec.doc_file.rwcmode",getspec_filemode,(char *) &write_mode},
    {"convert.doc.weight",        getspec_func,   (char *) &doc_weight.ptab},
    TRACE_PARAM ("vecwt_vec_obj.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


int
init_vecwt_vec_obj (spec, unused)
SPEC *spec;
char *unused;
{
    CONTEXT old_context;

    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_vecwt_vec_obj");

    old_context = get_context();
    set_context (CTXT_DOC);
    if (UNDEF == (doc_weight.inst =
                  doc_weight.ptab->init_proc (spec, (char *) NULL)))
        return (UNDEF);
    set_context (old_context);

    PRINT_TRACE (2, print_string, "Trace: leaving init_vecwt_vec_obj");

    return (0);
}

int
vecwt_vec_obj (in_vec_file, out_vec_file, inst)
char *in_vec_file;
char *out_vec_file;
int inst;
{
    VEC vector;
    int status;
    float out_sum = 0.0;
    int in_index, out_index;
    long i;

    PRINT_TRACE (2, print_string, "Trace: entering vecwt_vec_obj");

    if (in_vec_file == NULL)
        in_vec_file = default_vector_file;
    if (UNDEF == (in_index = open_vector (in_vec_file, read_mode))){
        return (UNDEF);
    }

    if (! VALID_FILE (out_vec_file)) {
        set_error (SM_ILLPA_ERR, "vecwt_vec_obj", out_vec_file);
        return (UNDEF);
    }
    if (UNDEF == (out_index = open_vector (out_vec_file, write_mode))){
        return (UNDEF);
    }

    /* Copy each vector in turn */
    vector.id_num.id = 0;
    EXT_NONE(vector.id_num.ext);
    if (global_start > 0) {
        vector.id_num.id = global_start;
        if (UNDEF == seek_vector (in_index, &vector)) {
            return (UNDEF);
        }
    }

    while (1 == (status = read_vector (in_index, &vector)) &&
           vector.id_num.id <= global_end) {

	if (UNDEF == doc_weight.ptab->proc (&vector,
                                            (char *) NULL,
                                            doc_weight.inst)) {
            return (UNDEF);
        }

#if 0
        // Don't know why this is being done. -mm (14/07/2006)
	out_sum = 0.0;
	for (i = 0; i < vector.ctype_len[0]; i++)
	    out_sum += vector.con_wtp[i].wt;
	for (i = 0; i < vector.num_conwt; i++)
	    vector.con_wtp[i].wt /= out_sum/(float) vector.ctype_len[0];
#endif
        if (UNDEF == seek_vector (out_index, &vector) ||
            1 != write_vector (out_index, &vector))
            return (UNDEF);
    }

    if (UNDEF == close_vector (out_index) || 
        UNDEF == close_vector (in_index))
        return (UNDEF);
    
    PRINT_TRACE (2, print_string, "Trace: leaving print_vecwt_vec_obj");
    return (status);
}

int
close_vecwt_vec_obj(inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_vecwt_vec_obj");

    if (UNDEF == doc_weight.ptab->close_proc(doc_weight.inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering close_vecwt_vec_obj");
    return (0);
}
