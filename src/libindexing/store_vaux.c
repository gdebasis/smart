#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/store_vaux.c,v 11.2 1993/02/03 16:51:06 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 store indexed vector in vector file plus ctype dependent auxiliary files
 *1 index.store.store_vec_aux
 *2 store_vec_aux (vec, unused, inst)
 *3   SM_VECTOR *vec;
 *3   char *unused;
 *3   int inst;
 *4 init_store_vec_aux (spec, unused)
 *5   "index.store.trace"
 *5   "index.ctype.*.store_aux"
 *5   "index.doc_file"
 *5   "index.doc_file.rwmode"
 *4 close_store_vec_aux (inst)
 *6 global_context is checked to make sure indexing document(CTXT_INDEXING_DOC)
 *7 The indexed vector vec is stored in aux files (normally inverted), and is
 *7 also stored as a vector in doc_file.
 *7 The method of storing a ctype of vec is ctype
 *7 dependant and is given by index.ctype.N.store_aux, where N is the ctype
 *7 involved.  It is an error to store anything except a new document.
 *7 UNDEF returned if error, else 1.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "docindex.h"
#include "vector.h"
#include "docdesc.h"
#include "trace.h"
#include "io.h"
#include "context.h"
#include "inst.h"

static char *vec_file;         /* Vector object in VEC format */
static long  vec_mode;         /* Mode to open vec_file */

static SPEC_PARAM spec_args[] = {
    {"index.doc_file",          getspec_dbfile,  (char *) &vec_file},
    {"index.doc_file.rwmode",   getspec_filemode,(char *) &vec_mode},
    TRACE_PARAM ("index.output.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    SM_INDEX_DOCDESC doc_desc;
    int *store_inst;

    int vec_fd;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static int save_spec_id = 0;

int
init_store_vec_aux (spec, unused)
SPEC *spec;
char *unused;
{
    int new_inst;
    STATIC_INFO *ip;
    long i;
    char param_prefix[PATH_LEN];

    if (! check_context (CTXT_DOC)) {
        set_error (SM_ILLPA_ERR, "Illegal context", "store_vec_aux");
        return (UNDEF);
    }

     /* Lookup the values of the relevant global parameters */
    if (spec->spec_id != save_spec_id &&
        UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);
    save_spec_id = spec->spec_id;

    PRINT_TRACE (2, print_string, "Trace: entering init_store_vec_aux");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    ip = &info[new_inst];

     if (UNDEF == lookup_spec_docdesc (spec, &ip->doc_desc))
        return (UNDEF);

    /* Reserve space for the instantiation ids of the called procedures. */
    if (NULL == (ip->store_inst = (int *)
                 malloc ((unsigned) ip->doc_desc.num_ctypes * sizeof (int))))
        return (UNDEF);

    for (i = 0; i < ip->doc_desc.num_ctypes; i++) {
        /* Set param_prefix to be current parameter path for this ctype.
           This will then be used by the store routine to lookup whatever
           parameters it needs. */
        (void) sprintf (param_prefix, "index.ctype.%ld.", i);
        if (UNDEF == (ip->store_inst[i] =
                      ip->doc_desc.ctypes[i].store_aux->init_proc
                      (spec, param_prefix)))
            return (UNDEF);
    }

    if (UNDEF == (ip->vec_fd = open_vector (vec_file, vec_mode))) {
        clr_err();
        if (UNDEF == (ip->vec_fd = open_vector (vec_file,
                                                vec_mode | SCREATE)))
            return (UNDEF);
    }

    ip->valid_info++;

    PRINT_TRACE (2, print_string, "Trace: leaving init_store_vec_aux");

    return (new_inst);
}

int
store_vec_aux (vec, unused, inst)
SM_VECTOR *vec;
char *unused;
int inst;
{
    VEC ctype_vec;
    long i;
    STATIC_INFO *ip;
    CON_WT *con_wtp;

    PRINT_TRACE (2, print_string, "Trace: entering store_vec_aux");
    PRINT_TRACE (6, print_vector, vec);

     if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "store_vec_aux");
        return (UNDEF);
    }
    ip  = &info[inst];

   if (vec->num_conwt == 0)
        return (0);

    /* Sanity checking */
    if (vec->num_ctype > ip->doc_desc.num_ctypes) {
        set_error (SM_INCON_ERR, "Too many ctypes", "store_aux");
        return (UNDEF);
    }

    if (UNDEF == seek_vector (ip->vec_fd, vec) ||
        UNDEF == write_vector (ip->vec_fd, vec)) {
        return (UNDEF);
    }

    ctype_vec.id_num = vec->id_num;
    ctype_vec.num_ctype = 1;
    con_wtp = vec->con_wtp;
    for (i = 0; i < vec->num_ctype; i++) {
        ctype_vec.ctype_len = &vec->ctype_len[i];
        ctype_vec.num_conwt = vec->ctype_len[i];
        ctype_vec.con_wtp   = con_wtp;
        if (UNDEF == ip->doc_desc.ctypes[i].store_aux->proc(&ctype_vec,
                                                            (char *) NULL,
                                                            ip->store_inst[i]))
            return (UNDEF);
        con_wtp += ctype_vec.num_conwt;
    }

    PRINT_TRACE (2, print_string, "Trace: leaving store_vec_aux");

    return (1);
}

int
close_store_vec_aux (inst)
int inst;
{
    STATIC_INFO *ip;
    long i;

    PRINT_TRACE (2, print_string, "Trace: entering close_store_vec_aux");

     if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_store_vec_aux");
        return (UNDEF);
    }
    ip  = &info[inst];
    ip->valid_info--;

    if (UNDEF == close_vector (ip->vec_fd))
        return (UNDEF);

     for (i = 0; i < ip->doc_desc.num_ctypes; i++) {
        if (UNDEF == ip->doc_desc.ctypes[i].store_aux->close_proc
            (ip->store_inst[i]))
            return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_store_vec_aux");

    return (0);
}
