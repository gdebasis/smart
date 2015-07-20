#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/dgetdoc_vec.c,v 11.0 1992/07/21 18:20:09 chrisb Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Return the next doc to be searched, from document vector
 *1 local.retrieve.get_doc.vec
 *2 getdoc_vec (NULL, vec, inst)
 *3 char *NULL;
 *3 VEC *vec;
 *3 int inst;
 *4 init_getdoc_vec (spec, unused)
 *5   "retrieve.doc_file"
 *5   "retrieve.doc_file.rmode"
 *5   "retrieve.get_doc.trace"
 *4 close_getdoc_vec(inst)
 *7 getdoc_vec returns the next document to be searched during retrieval.
 *7 It goes sequentially through the document vectors specified 
 *7 by doc_file.  No processing is done on the document.
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

static char *vec_file;
static long vec_file_mode;

static SPEC_PARAM spec_args[] = {
    {"retrieve.doc_file",        getspec_dbfile, (char *) &vec_file},
    {"retrieve.doc_file.rmode",  getspec_filemode, (char *) &vec_file_mode},
    TRACE_PARAM ("retrieve.get_doc.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    int vec_fd;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;


int
init_getdoc_vec (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_getdoc_vec");
    
    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];
    
    if (! VALID_FILE (vec_file)) {
        set_error (SM_ILLPA_ERR, vec_file, "init_getdoc_vec");
        return (UNDEF);
    }

    if (UNDEF == (ip->vec_fd = open_vector (vec_file, vec_file_mode)))
        return (UNDEF);

    ip->valid_info = 1;
             
    PRINT_TRACE (2, print_string, "Trace: leaving init_getdoc_vec");
    
    return (new_inst);
}


int
getdoc_vec (did, vec, inst)
long *did;
VEC *vec;
int inst;
{
    STATIC_INFO *ip;
    int status;

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "getdoc_vec");
        return (UNDEF);
    }
    ip  = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering getdoc_vec");

    if (ip->vec_fd == UNDEF) {
        set_error (SM_INCON_ERR, "Vector not initialized", "getdoc_vec");
        return (UNDEF);
    }

    /* If did is non-NULL, then that is the desired document */
    if (did != (long *) NULL) {
        vec->id_num.id = *did;
        if (1 != (status = seek_vector (ip->vec_fd, vec)))
            return (status);
    }

    /* read vector from vec_file */
    if (1 != (status = read_vector (ip->vec_fd, vec))) {
        return (status);
    }

    PRINT_TRACE (4, print_vector, vec);
    PRINT_TRACE (2, print_string, "Trace: leaving getdoc_vec");
    return (1);
}

int
close_getdoc_vec(inst)
int inst;
{

    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_getdoc_vec");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_getdoc_vec");
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
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_getdoc_vec");
    return (0);
}
