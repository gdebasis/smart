#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/vec_inv_o.c,v 11.2 1993/02/03 16:49:17 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Take a vector relational object, and convert to inverted file. Obsolete.
 *1 convert.obj.vec_inv_obj
 *2 vec_inv_obj (vector_file, inv_file, inst)
 *3   char *vector_file;
 *3   char *inv_file;
 *3   int inst;

 *4 init_vec_inv_obj (spec, unused)
 *5   "convert.low.vec_inv"
 *5   "convert.vec_inv.doc_file"
 *5   "convert.vec_inv.doc_file.rmode"
 *5   "vec_inv_obj.trace"
 *4 close_vec_inv_obj(inst)

 *6 global_start and global_end are checked.  Only those vectors with
 *6 id_num falling between them are converted.
 *7 Read vector tuples from vector file and add to inv_file using the
 *7 procedure "vec_inv".
 *7 If vector_file is NULL then doc_file is used.
 *7 If inv_file is NULL, then the default file determined by "vec_inv" is used.
 *7 Return UNDEF if error, else 0;
 *9 Obsolete.  Converts entire vector to inverted file, ignoring ctypes.
 *9 In most cases should use vec_aux_obj instead, which allows ctype
 *9 dependant storage.
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

static PROC_INST pvec_inv;

static char *default_vector_file;
static long vector_mode;


static SPEC_PARAM spec_args[] = {
    {"convert.low.vec_inv",       getspec_func,   (char *) &pvec_inv.ptab},
    {"convert.vec_inv.doc_file",  getspec_dbfile, (char *) &default_vector_file},
    {"convert.vec_inv.doc_file.rmode",getspec_filemode, (char *) &vector_mode},
    TRACE_PARAM ("vec_inv_obj.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


static int vector_index;
static SPEC save_spec;

int
init_vec_inv_obj (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_vec_inv_obj");

    (void) copy_spec (&save_spec, spec);

    PRINT_TRACE (2, print_string, "Trace: leaving init_vec_inv_obj");

    return (0);
}
int
vec_inv_obj (vector_file, inv_file, inst)
char *vector_file;
char *inv_file;
int inst;
{
    VEC vector;
    int status;
    char *inv_file_spec_mod[2];

    PRINT_TRACE (2, print_string, "Trace: entering vec_inv_obj");

    if (vector_file == NULL)
        vector_file = default_vector_file;
    if (UNDEF == (vector_index = open_vector (vector_file, vector_mode))){
        return (UNDEF);
    }

    if (inv_file != NULL) {
        inv_file_spec_mod[0] = "inv_file";
        inv_file_spec_mod[1] = inv_file;
        if (UNDEF == mod_spec (&save_spec, 2, &inv_file_spec_mod[0]))
            return (UNDEF);
    }

    if (UNDEF == (pvec_inv.inst =
                  pvec_inv.ptab->init_proc (&save_spec, ""))) {
        return (UNDEF);
    }

    /* Convert each vector in turn */
    vector.id_num.id = 0;
    EXT_NONE(vector.id_num.ext);
    if (global_start > 0) {
        vector.id_num.id = global_start;
        if (UNDEF == seek_vector (vector_index, &vector)) {
            return (UNDEF);
        }
    }

    while (1 == (status = read_vector (vector_index, &vector)) &&
           vector.id_num.id <= global_end) {
        if (1 != pvec_inv.ptab->proc (&vector, (char *) NULL, pvec_inv.inst)) {
            return (UNDEF);
        }
    }

    if (UNDEF == pvec_inv.ptab->close_proc (pvec_inv.inst))
        return (UNDEF);

    if (UNDEF == close_vector (vector_index))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving print_vec_inv_obj");
    return (status);
}

int
close_vec_inv_obj(inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_vec_inv_obj");

    PRINT_TRACE (2, print_string, "Trace: entering close_vec_inv_obj");
    return (0);
}
