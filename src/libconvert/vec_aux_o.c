#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/vec_aux_o.c,v 11.2 1993/02/03 16:49:30 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Take a vector relational object, and convert to auxiliary files.
 *1 convert.obj.vec_aux_obj
 *2 vec_aux_obj (vector_file, inv_file, inst)
 *3   char *vector_file;
 *3   char *inv_file;
 *3   int inst;

 *4 init_vec_aux_obj (spec, unused)
 *5   "convert.vec_aux.doc_file"
 *5   "convert.vec_aux.doc_file.rmode"
 *5   "convert.vec_aux.trace"
 *5   "index.ctype.*.store_aux"
 *4 close_vec_aux_obj(inst)

 *6 global_start and global_end are checked.  Only those vectors with
 *6 id_num falling between them are converted.

 *7 Read vector tuples from vector file, break vector into separate ctypes,
 *7 and add each ctype N to the indicated aux_file using the ctype-dependant
 *7 procedure given by index.ctype.N.store_aux.
 *7 If vector_file is NULL then doc_file is used.
 *7 If inv_file is non-NULL, then the value of the spec parameter "inv_file"
 *7 is set to inv_file.  the store_aux procedure will look up the
 *7 parameter giving the aux_file it is interested in.  In the most common
 *7 case, the store_aux procedure is "vec_inv", which then will look up the
 *7 value of "ctype.N.inv_file", which may just have been set to inv_file.
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
#include "docdesc.h"

static char *default_vector_file;
static long vector_mode;


static SPEC_PARAM spec_args[] = {
    {"convert.vec_aux.doc_file",  getspec_dbfile, (char *) &default_vector_file},
    {"convert.vec_aux.doc_file.rmode",getspec_filemode, (char *) &vector_mode},
    TRACE_PARAM ("convert.vec_aux.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


static int vector_index;
static SPEC save_spec;
static SM_INDEX_DOCDESC doc_desc;

int
init_vec_aux_obj (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_vec_aux_obj");

    (void) copy_spec (&save_spec, spec);
    if (UNDEF == lookup_spec_docdesc (spec, &doc_desc))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_vec_aux_obj");

    return (0);
}

int
vec_aux_obj (vector_file, inv_file, inst)
char *vector_file;
char *inv_file;
int inst;
{
    VEC vector, ctype_vec;
    int status;
    char *inv_file_spec_mod[2];
    int *store_inst;
    CON_WT *con_wtp;
    long i;
    char param_prefix[PATH_LEN];

    PRINT_TRACE (2, print_string, "Trace: entering vec_aux_obj");

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

    /* Reserve space for the instantiation ids of the called procedures. */
    if (NULL == (store_inst = (int *)
                 malloc ((unsigned) doc_desc.num_ctypes * sizeof (int))))
        return (UNDEF);

     for (i = 0; i < doc_desc.num_ctypes; i++) {
        /* Set param_prefix to be current parameter path for this ctype.
           This will then be used by the store routine to lookup whatever
           parameters it needs. */
        (void) sprintf (param_prefix, "index.ctype.%ld.", i);
        if (UNDEF == (store_inst[i] =
                      doc_desc.ctypes[i].store_aux->init_proc
                      (&save_spec, param_prefix)))
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
        if (vector.num_ctype > doc_desc.num_ctypes) {
            set_error (SM_INCON_ERR, "Too many ctypes", "vec_aux_obj");
            return (UNDEF);
        }
        ctype_vec.id_num = vector.id_num;
        ctype_vec.num_ctype = 1;
        con_wtp = vector.con_wtp;
        for (i = 0; i < vector.num_ctype; i++) {
            ctype_vec.ctype_len = &vector.ctype_len[i];
            ctype_vec.num_conwt = vector.ctype_len[i];
            ctype_vec.con_wtp   = con_wtp;
            if (UNDEF == doc_desc.ctypes[i].store_aux->proc (&ctype_vec,
                                                             (char *) NULL,
                                                             store_inst[i]))
                return (UNDEF);
            con_wtp += ctype_vec.num_conwt;
        }
    }
    
    if (UNDEF == close_vector (vector_index))
        return (UNDEF);

    for (i = 0; i < doc_desc.num_ctypes; i++) {
        if (UNDEF == doc_desc.ctypes[i].store_aux->close_proc (store_inst[i]))
            return (UNDEF);
    }

    (void) free ((char *) store_inst);


    PRINT_TRACE (2, print_string, "Trace: leaving print_vec_aux_obj");
    return (status);
}

int
close_vec_aux_obj(inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_vec_aux_obj");

    PRINT_TRACE (2, print_string, "Trace: entering close_vec_aux_obj");
    return (0);
}
