#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/dgetdoc_vec.c,v 11.0 1992/07/21 18:20:09 chrisb Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Return the next doc to be searched, reweighting document vector
 *2 getdoc_wtvec (did, vec, inst)
 *3 long *did;
 *3 VEC *vec;
 *3 int inst;
 *4 init_getdoc_wtvec (spec, unused)
 *5   "retrieve.nnn_doc_file"
 *5   "retrieve.nnn_doc_file.rmode"
 *5   "retrieve.doc.weight"
 *5   "retrieve.getdoc.trace"
 *4 close_getdoc_wtvec(inst)
 *7 getdoc_wtvec returns the next document to be searched during retrieval.
 *7 It goes sequentially through the document vectors (normally tf weighted
 *7 specified by nnn_doc_file.  Each vector is then reweighted according to
 *7 procedure doc.weight.
 *7 1 is returned if vec is assigned a valid document.
 *7 0 is returned if there are no more documents in the relation.
 *7 -1 is returned if error.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "vector.h"
#include "inst.h"
#include "context.h"

static char *vec_file;
static long vec_file_mode;
static PROC_TAB *doc_weight;

static SPEC_PARAM spec_args[] = {
    {"retrieve.nnn_doc_file",    getspec_dbfile, (char *) &vec_file},
    {"retrieve.nnn_doc_file.rmode",getspec_filemode, (char *) &vec_file_mode},
    {"retrieve.doc.weight",        getspec_func,   (char *) &doc_weight},
    TRACE_PARAM ("retrieve.getdoc.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    int vec_fd;
    PROC_INST doc_weight;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;


int
init_getdoc_wtvec (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;
    CONTEXT old_context;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_getdoc_wtvec");
    
    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];
    
    if (! VALID_FILE (vec_file)) {
        set_error (SM_ILLPA_ERR, vec_file, "init_getdoc_wtvec");
        return (UNDEF);
    }

    if (UNDEF == (ip->vec_fd = open_vector (vec_file, vec_file_mode)))
        return (UNDEF);

    old_context = get_context();
    set_context (CTXT_DOC);
    ip->doc_weight.ptab = doc_weight;
    if (UNDEF == (ip->doc_weight.inst =
                  doc_weight->init_proc (spec, (char *) NULL)))
        return (UNDEF);
    set_context (old_context);

    ip->valid_info = 1;
             
    PRINT_TRACE (2, print_string, "Trace: leaving init_getdoc_wtvec");
    
    return (new_inst);
}


int
getdoc_wtvec (did, vec, inst)
long *did;
VEC *vec;
int inst;
{
    STATIC_INFO *ip;
    int status;
    VEC nnn_vec;      /* Unweighted vector read from nnn_doc_file */

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "getdoc_wtvec");
        return (UNDEF);
    }
    ip  = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering getdoc_wtvec");

    if (ip->vec_fd == UNDEF) {
        set_error (SM_INCON_ERR, "Vector not initialized", "getdoc_wtvec");
        return (UNDEF);
    }

    /* If did is non-NULL, then that is the desired document */
    if (did != (long *) NULL) {
        nnn_vec.id_num.id = *did;
        if (1 != (status = seek_vector (ip->vec_fd, &nnn_vec)))
            return (status);
    }

    /* read vector from vec_file */
    if (1 != (status = read_vector (ip->vec_fd, &nnn_vec))) {
        return (status);
    }

    /* Weight the vector */
    if (UNDEF == ip->doc_weight.ptab->proc (&nnn_vec,
                                            vec,
                                            ip->doc_weight.inst)) {
        return (UNDEF);
    }

    PRINT_TRACE (4, print_vector, vec);
    PRINT_TRACE (2, print_string, "Trace: leaving getdoc_wtvec");
    return (1);
}

int
close_getdoc_wtvec(inst)
int inst;
{

    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_getdoc_wtvec");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_getdoc_wtvec");
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
        if (UNDEF == ip->doc_weight.ptab->close_proc (ip->doc_weight.inst))
            return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_getdoc_wtvec");
    return (0);
}
