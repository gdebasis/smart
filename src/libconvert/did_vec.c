#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/did_vec.c,v 11.0 1992/07/21 18:20:09 chrisb Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Given a document id, return the corresponding document vector
 *2 did_vec (did, vec, inst)
 *3 long *did;
 *3 VEC *vec;
 *3 int inst;
 *4 init_did_vec (spec, unused)
 *5   "convert.doc.index_vec"
 *5   "convert.doc_file"
 *5   "convert.doc_file.rmode"
 *5   "convert.id_vec.trace"
 *5
 *5   "convert.nnn_doc_file"
 *5   "convert.nnn_doc_file.rmode"
 *5   "convert.doc.weight"
 *5
 *4 close_did_vec(inst)
 *6 global_context is temporarily set to indicate indexing doc (CTXT_DOC)
 *7 did_vec will read the vector corresponding to did from doc_file if that 
 *7 vector file exists.  If it does not, then the vector is read from
 *7 a term-freq weighted doc-file (nnn_doc_file) and correctly weighted
 *7 before returning.  If nnn_doc_file does not exist, then the text of 
 *7 the document is obtained (location given by textloc_file), and 
 *7 the document is indexed from scratch.
 *7 Return 0 if the document is not found, return 1 if found, UNDEF if error
 *8 Most of code shared with qid_vec; global_context indicates which is
 *8 being run.
***********************************************************************/


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Given a query id, return the corresponding query vector
 *2 qid_vec (qid, vec, inst)
 *3 long *qid;
 *3 VEC *vec;
 *3 int inst;
 *4 init_qid_vec (spec, unused)
 *5   "convert.query.index_vec"
 *5   "convert.query_file"
 *5   "convert.query_file.rmode"
 *5   "convert.id_vec.trace"
 *4 close_qid_vec(inst)
 *6 global_context is temporarily set to indicate indexing query (CTXT_QUERY)
 *7 qid_vec will read the vector corresponding to qid from query_file if that 
 *7 vector file exists.  If it does not, then the text of the query is
 *7 obtained (location given by query.textloc_file), and the query is indexed
 *7 from scratch.
 *7 Return 0 if the query is not found, return 1 if found, UNDEF if error
 *8 Most of code shared with did_vec; global_context indicates which is
 *8 being run.
***********************************************************************/

#include <fcntl.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "context.h"
#include "retrieve.h"
#include "vector.h"
#include "docindex.h"
#include "inst.h"

/* Convert the given docid to a vector, either by reading doc_file, or
   if that doesn't exist, by reading the original text of the doc
   and re-indexing it. */


static PROC_TAB *index_vec;       /* Indexing procedures */

static char *vec_file;
static long vec_file_mode;


static SPEC_PARAM spec_args[] = {
    {"convert.doc.index_vec",   getspec_func, (char *) &index_vec},
    {"convert.doc_file",        getspec_dbfile, (char *) &vec_file},
    {"convert.doc_file.rmode",  getspec_filemode, (char *) &vec_file_mode},
    TRACE_PARAM ("convert.id_vec.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static char *nnn_vec_file;
static long nnn_vec_file_mode;
static PROC_TAB *doc_weight;

static SPEC_PARAM nnn_spec_args[] = {
    {"convert.nnn_doc_file",    getspec_dbfile, (char *) &nnn_vec_file},
    {"convert.nnn_doc_file.rmode",getspec_filemode, (char *) &nnn_vec_file_mode},
    {"convert.doc.weight",        getspec_func,   (char *) &doc_weight},
    };
static int num_nnn_spec_args = sizeof (nnn_spec_args) /
         sizeof (nnn_spec_args[0]);

static SPEC_PARAM query_spec_args[] = {
    {"convert.query.index_vec",   getspec_func, (char *) &index_vec},
    {"convert.query_file",        getspec_dbfile, (char *) &vec_file},
    {"convert.query_file.rmode",  getspec_filemode, (char *) &vec_file_mode},
    TRACE_PARAM ("convert.id_vec.trace")
    };
static int num_query_spec_args = sizeof (query_spec_args) /
         sizeof (query_spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    int vec_fd;
    int nnn_vec_fd;
    int spec_id;
    PROC_INST index_vec;
    PROC_INST doc_weight;
    int indexing_doc_flag;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static int init_id_vec(), id_vec(), close_id_vec();


int
init_did_vec (spec, unused)
SPEC *spec;
char *unused;
{
    CONTEXT old_context;
    int inst;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    /* Look up the optional nnn_doc_file parameter.  Not an */
    /* error if no default value */
    nnn_vec_file = (char *) NULL;
    if (UNDEF == lookup_spec (spec, &nnn_spec_args[0], num_nnn_spec_args)) {
        clr_err();
    }

    old_context = get_context();
    set_context (CTXT_DOC);
    if (UNDEF == (inst = init_id_vec (spec, "convert.doc.")))
        return (UNDEF);
    set_context (old_context);
    nnn_vec_file = (char *) NULL;

    return (inst);
}

int
did_vec (did, vec, inst)
long *did;
VEC *vec;
int inst;
{
    return (id_vec (did, vec, inst));
}

int
close_did_vec(inst)
int inst;
{
    return (close_id_vec (inst));
}


int
init_qid_vec (spec, unused)
SPEC *spec;
char *unused;
{
    CONTEXT old_context;
    int inst;

    if (UNDEF == lookup_spec (spec, &query_spec_args[0], num_query_spec_args))
        return (UNDEF);

    old_context = get_context();
    set_context (CTXT_QUERY);
    if (UNDEF == (inst = init_id_vec (spec, "convert.query.")))
        return (UNDEF);
    set_context (old_context);

    return (inst);
}

int
qid_vec (qid, vec, inst)
long *qid;
VEC *vec;
int inst;
{
    return (id_vec (qid, vec, inst));
}

int
close_qid_vec(inst)
int inst;
{
    return (close_id_vec (inst));
}


static int
init_id_vec (spec, qd_prefix)
SPEC *spec;
char *qd_prefix;
{
    long i;
    STATIC_INFO *ip;
    int new_inst;

    /* Check to see if this spec has already been initialized.  If
       so, that instantiation will be used. */
    for (i = 0; i < max_inst; i++) {
        if (info[i].valid_info && spec->spec_id == info[i].spec_id &&
            check_context (CTXT_DOC) == info[i].indexing_doc_flag) {
            info[i].valid_info++;
            PRINT_TRACE (2, print_string,
                         "Trace: entering/leaving init_id_vec");
            return (i);
        }
    }
    
    PRINT_TRACE (2, print_string, "Trace: entering init_id_vec");
    
    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];
    
    ip->spec_id = spec->spec_id;

    if (VALID_FILE (vec_file)) {
        ip->nnn_vec_fd = UNDEF;
        if (UNDEF == (ip->vec_fd = open_vector (vec_file, vec_file_mode)))
            return (UNDEF);
    }
    else if (VALID_FILE (nnn_vec_file)) {
        ip->vec_fd = UNDEF;
        if (UNDEF == (ip->nnn_vec_fd = open_vector (nnn_vec_file,
                                                    nnn_vec_file_mode)))
            return (UNDEF);
        ip->doc_weight.ptab = doc_weight;
        if (UNDEF == (ip->doc_weight.inst =
                      doc_weight->init_proc (spec, qd_prefix)))
            return (UNDEF);
    }
    else {
        ip->vec_fd = UNDEF;
        ip->nnn_vec_fd = UNDEF;

        /* Call all initialization procedures for indexing */
        ip->index_vec.ptab = index_vec;
        ip->index_vec.inst = index_vec->init_proc (spec, qd_prefix);
        if (UNDEF == ip->index_vec.inst)
            return (UNDEF);
    }

    ip->indexing_doc_flag = check_context (CTXT_DOC);

    ip->valid_info = 1;
             
    PRINT_TRACE (2, print_string, "Trace: leaving init_id_vec");
    
    return (new_inst);
}


static int
id_vec (vecid, vec, inst)
long *vecid;
VEC *vec;
int inst;
{
    STATIC_INFO *ip;
    EID text_id;
    VEC nnn_vec;
    int status;

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "id_vec");
        return (UNDEF);
    }
    ip  = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering id_vec");
    PRINT_TRACE (6, print_long, vecid);

    if (ip->vec_fd != UNDEF) {
        /* read vector from vec_file */
        vec->id_num.id = *vecid;
	EXT_NONE(vec->id_num.ext);
        if (1 != (status = seek_vector (ip->vec_fd, vec)) ||
            1 != (status = read_vector (ip->vec_fd, vec))) {
            if (status == UNDEF)
                return (UNDEF);
        }
        PRINT_TRACE (4, print_vector, vec);
        PRINT_TRACE (2, print_string, "Trace: leaving id_vec");
        return (status);
    }

    if (ip->nnn_vec_fd != UNDEF) {
        /* read vector from nnn_vec_file */
        nnn_vec.id_num.id = *vecid;
	EXT_NONE(nnn_vec.id_num.ext);
        if (1 != (status = seek_vector (ip->nnn_vec_fd, &nnn_vec)) ||
            1 != (status = read_vector (ip->nnn_vec_fd, &nnn_vec))) {
            if (status == UNDEF)
                return (UNDEF);
        }
        if (status == 1 &&
            UNDEF == ip->doc_weight.ptab->proc (&nnn_vec,
                                                vec,
                                                ip->doc_weight.inst)) {
            return (UNDEF);
        }

        PRINT_TRACE (4, print_vector, vec);
        PRINT_TRACE (2, print_string, "Trace: leaving id_vec");
        return (status);
    }

    text_id.id = *vecid;
    EXT_NONE(text_id.ext);

    /* Index text_id */
    status = ip->index_vec.ptab->proc (&text_id, vec, ip->index_vec.inst);
    if (UNDEF == status)
        return (UNDEF);

    PRINT_TRACE (4, print_vector, vec);
    PRINT_TRACE (2, print_string, "Trace: leaving id_vec");
    return (status);
}


static int
close_id_vec(inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_id_vec");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_id_vec");
        return (UNDEF);
    }

    ip  = &info[inst];
    ip->valid_info--;
    /* Output everything and free buffers if this was last close of this
       inst */
    if (ip->valid_info == 0) {
        if (UNDEF != ip->vec_fd) {
            if (UNDEF == close_vector (ip->vec_fd))
                return (UNDEF);
            ip->vec_fd = UNDEF;
        }
        else if (UNDEF != ip->nnn_vec_fd) {
            if (UNDEF == close_vector (ip->nnn_vec_fd))
                return (UNDEF);
            ip->nnn_vec_fd = UNDEF;
            if (UNDEF == ip->doc_weight.ptab->close_proc (ip->doc_weight.inst))
                return (UNDEF);
        }
        else {
            if (UNDEF == ip->index_vec.ptab->close_proc (ip->index_vec.inst))
                return (UNDEF);
        }
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_id_vec");
    return (0);
}
