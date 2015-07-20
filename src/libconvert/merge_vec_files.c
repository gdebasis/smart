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
 *1 convert.obj.merge_vec_files
 *2 merge_vec_files (unused, out_vec_file, inst)
 *3   char *unused;
 *3   char *out_vec_file;
 *3   int inst;

 *4 init_merge_vec_files (spec, unused)
 *5   "convert.vec_vec.doc_file"
 *5   "convert.vec_vec.doc_file.rmode"
 *5   "convert.vec_vec.doc_file.rwcmode"
 *5   "merge_vec_files.trace"
 *4 close_merge_vec_files(inst)

 *6 global_start and global_end are checked.  Only those vectors with
 *6 id_num falling between them are converted.
 *7 Read vector tuples from in_vec_file and copy to out_vec_file.
 *7 If out_vec_file is NULL, then error.
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

static char *vec_file1;
static char *vec_file2;
static long read_mode;
static long write_mode;

static SPEC_PARAM spec_args[] = {
    {"convert.merge_vec.vec_file1",  getspec_dbfile, (char *) &vec_file1},
    {"convert.merge_vec.vec_file2",  getspec_dbfile, (char *) &vec_file2},
    {"convert.vec_vec.doc_file.rmode",getspec_filemode, (char *) &read_mode},
    {"convert.vec_vec.doc_file.rwcmode",getspec_filemode, (char *) &write_mode},
    TRACE_PARAM ("merge_vec_files.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


int
init_merge_vec_files (spec, unused)
SPEC *spec;
char *unused;
{
    PRINT_TRACE (2, print_string, "Trace: entering init_merge_vec_files");

    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving init_merge_vec_files");

    return (0);
}

int
merge_vec_files (unused, out_vec_file, inst)
char *unused;
char *out_vec_file;
int inst;
{
    VEC vector, vector2, outvec;
    int status;
    int in_index_1, in_index_2, out_index;
    long did_index;

    PRINT_TRACE (2, print_string, "Trace: entering merge_vec_files");

    if (UNDEF == (in_index_1 = open_vector (vec_file1, read_mode))){
        return (UNDEF);
    }
    if (UNDEF == (in_index_2 = open_vector (vec_file2, read_mode))){
        return (UNDEF);
    }

    if (! VALID_FILE (out_vec_file)) {
        set_error (SM_ILLPA_ERR, "merge_vec_files", out_vec_file);
        return (UNDEF);
    }
    if (UNDEF == (out_index = open_vector (out_vec_file, write_mode))){
        return (UNDEF);
    }

    /* Copy each vector in turn */
    vector.id_num.id = 0;
    EXT_NONE(vector.id_num.ext);
    vector2.id_num.id = 0;
    EXT_NONE(vector2.id_num.ext);
    if (global_start > 0) {
        vector.id_num.id = global_start;
        vector2.id_num.id = global_start;
        if (UNDEF == seek_vector (in_index_1, &vector))
			return UNDEF;
		if (UNDEF == seek_vector (in_index_2, &vector2))
            return (UNDEF);
    }

    did_index = 0;
    while (1) {
		if ((1 != (status = read_vector (in_index_1, &vector))) || vector.id_num.id > global_end) {
			break;
		}
		if ((1 != (status = read_vector (in_index_2, &vector2))) || vector2.id_num.id > global_end) {
			break;
		}

		merge_vec(&vector, &vector2, &outvec);
        if (UNDEF == seek_vector (out_index, &outvec) ||
            1 != write_vector (out_index, &outvec))
            return (UNDEF);
		free_vec(&outvec);
    }

    if (UNDEF == close_vector (out_index) || 
        UNDEF == close_vector (in_index_1) ||
		 UNDEF == close_vector (in_index_2) )
        return (UNDEF);
    
    PRINT_TRACE (2, print_string, "Trace: leaving merge_vec_files");
    return (status);
}

int
close_merge_vec_files(inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_merge_vec_files");

    PRINT_TRACE (2, print_string, "Trace: entering close_merge_vec_files");
    return (0);
}
