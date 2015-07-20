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
 *1 convert.obj.vec_vec_obj
 *2 vec_vec_obj (in_vec_file, out_vec_file, inst)
 *3   char *in_vec_file;
 *3   char *out_vec_file;
 *3   int inst;

 *4 init_vec_vec_obj (spec, unused)
 *5   "convert.vec_vec.doc_file"
 *5   "convert.vec_vec.doc_file.rmode"
 *5   "convert.vec_vec.doc_file.rwcmode"
 *5   "convert.deleted_doc_file"
 *5   "vec_vec_obj.trace"
 *4 close_vec_vec_obj(inst)

 *6 global_start and global_end are checked.  Only those vectors with
 *6 id_num falling between them are converted.
 *7 Read vector tuples from in_vec_file and copy to out_vec_file.
 *7 If in_vec_file is NULL then doc_file is used.
 *7 If out_vec_file is NULL, then error.
 *7 If deleted_doc_file is a valid file, then it is assumed to be a
 *7 text file composed of docid's to be deleted during the copying process.
 *7 The docid's are assumed to be numerically sorted.
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
static char *deleted_doc_file;


static SPEC_PARAM spec_args[] = {
    {"convert.vec_vec.doc_file",  getspec_dbfile, (char *) &default_vector_file},
    {"convert.vec_vec.doc_file.rmode",getspec_filemode, (char *) &read_mode},
    {"convert.vec_vec.doc_file.rwcmode",getspec_filemode, (char *) &write_mode},
    {"convert.deleted_doc_file",  getspec_dbfile, (char *) &deleted_doc_file},
    TRACE_PARAM ("vec_vec_obj.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


int
init_vec_vec_obj (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_vec_vec_obj");

    PRINT_TRACE (2, print_string, "Trace: leaving init_vec_vec_obj");

    return (0);
}

int
vec_vec_obj (in_vec_file, out_vec_file, inst)
char *in_vec_file;
char *out_vec_file;
int inst;
{
    VEC vector;
    int status;
    int in_index, out_index;
    long *del_dids;
    long num_del_dids;
    long did_index;

    PRINT_TRACE (2, print_string, "Trace: entering vec_vec_obj");

    num_del_dids = 0;
    if (VALID_FILE (deleted_doc_file) &&
        UNDEF == text_long_array(deleted_doc_file, &del_dids, &num_del_dids))
        return (UNDEF);

    if (in_vec_file == NULL)
        in_vec_file = default_vector_file;
    if (UNDEF == (in_index = open_vector (in_vec_file, read_mode))){
        return (UNDEF);
    }

    if (! VALID_FILE (out_vec_file)) {
        set_error (SM_ILLPA_ERR, "vec_vec_obj", out_vec_file);
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

    did_index = 0;
    while (1 == (status = read_vector (in_index, &vector)) &&
           vector.id_num.id <= global_end) {
        /* See if document should be deleted */
        while (did_index < num_del_dids &&
               del_dids[did_index] < vector.id_num.id)
            did_index++;
        if (did_index < num_del_dids &&
            del_dids[did_index] == vector.id_num.id)
            continue;

        if (UNDEF == seek_vector (out_index, &vector) ||
            1 != write_vector (out_index, &vector))
            return (UNDEF);
    }

    if (UNDEF == close_vector (out_index) || 
        UNDEF == close_vector (in_index))
        return (UNDEF);
    if (VALID_FILE (deleted_doc_file) &&
        UNDEF == free_long_array (deleted_doc_file, &del_dids, &num_del_dids))
        return (UNDEF);
    
    
    PRINT_TRACE (2, print_string, "Trace: leaving print_vec_vec_obj");
    return (status);
}

int
close_vec_vec_obj(inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_vec_vec_obj");

    PRINT_TRACE (2, print_string, "Trace: entering close_vec_vec_obj");
    return (0);
}
