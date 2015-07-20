#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/store_vec.c,v 11.2 1993/02/03 16:51:07 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 store indexed vector in vector file
 *1 index.store.store_vec
 *2 store_vec (vec, unused, inst)
 *3   SM_VECTOR *vec;
 *3   char *unused;
 *3   int inst;
 *4 init_store_vec (spec, unused)
 *5   "index.store.trace"
 *5   "index.doc_file"
 *5   "index.doc_file.rwmode"
 *5   "index.query_file"
 *5   "index.query_file.rwmode"
 *4 close_store_vec_aux (inst)
 *6 global_context is checked to determine vector file to be used.
 *6 (CTXT_DOC -> doc_file, else (CTXT_QUERY) query_file)
 *7 The indexed vector vec is stored as a vector in designated vector file.
 *7 UNDEF returned if error, else 1.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "docindex.h"
#include "vector.h"
#include "trace.h"
#include "io.h"
#include "context.h"
#include "inst.h"

static char *vec_file;         /* Vector object in VEC format */
static long  vec_mode;         /* Mode to open vec_file */

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("index.output.trace")
    {"index.doc_file",          getspec_dbfile,  (char *) &vec_file},
    {"index.doc_file.rwmode",   getspec_filemode,(char *) &vec_mode},
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static SPEC_PARAM qspec_args[] = {
    TRACE_PARAM ("index.output.trace")
    {"index.query_file",          getspec_dbfile,  (char *) &vec_file},
    {"index.query_file.rwmode",   getspec_filemode,(char *) &vec_mode},
    };
static int num_qspec_args = sizeof (qspec_args) / sizeof (qspec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    int vec_fd;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_store_vec (spec, unused)
SPEC *spec;
char *unused;
{
    int new_inst;
    STATIC_INFO *ip;

    /* Lookup the values of the relevant parameters */
    if (check_context (CTXT_DOC)) {
        if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args)) {
            return (UNDEF);
        }
    }
    else {
        if (UNDEF == lookup_spec (spec, &qspec_args[0], num_qspec_args)) {
            return (UNDEF);
        }
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_store_vec");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    ip = &info[new_inst];

    if (UNDEF == (ip->vec_fd = open_vector (vec_file, vec_mode))) {
        clr_err();
        if (UNDEF == (ip->vec_fd = open_vector (vec_file,
                                                vec_mode | SCREATE)))
            return (UNDEF);
    }

    ip->valid_info++;

    PRINT_TRACE (2, print_string, "Trace: leaving init_store_vec");

    return (new_inst);

}

int
store_vec (vec, unused, inst)
SM_VECTOR *vec;
char *unused;
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering store_vec");
    PRINT_TRACE (6, print_vector, vec);

     if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "store_vec_aux");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (vec->num_conwt == 0)
        return (0);

    if (UNDEF == seek_vector (ip->vec_fd, vec) ||
        UNDEF == write_vector (ip->vec_fd, vec)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving store_vec");

    return (1);
}

int
close_store_vec (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_store_vec");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_store_vec");
        return (UNDEF);
    }
    ip  = &info[inst];
    ip->valid_info--;

    if (UNDEF == close_vector (ip->vec_fd)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_store_vec");

    return (0);
}
