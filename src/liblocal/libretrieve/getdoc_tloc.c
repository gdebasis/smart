#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/dgetdoc_vec.c,v 11.0 1992/07/21 18:20:09 chrisb Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Return the next doc to be searched, indexing from textloc file
 *2 getdoc_textloc (NULL, vec, inst)
 *3 char *NULL;
 *3 VEC *vec;
 *3 int inst;
 *4 init_getdoc_textloc (spec, unused)
 *5   "retrieve.doc.index_vec"
 *5   "retrieve.textloc_file"
 *5   "retrieve.textloc_file.rmode"
 *5   "retrieve.getdoc.trace"
 *4 close_getdoc_textloc(inst)
 *7 getdoc_textloc returns the next document to be searched during retrieval.
 *7 It goes sequentially through the specified textloc file.
 *7 Each document is re-indexed from scratch and returned as a vector.
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
#include "docindex.h"
#include "textloc.h"
#include "inst.h"
#include "context.h"

static PROC_TAB *index_vec;       /* Indexing procedures */
static char *textloc_file;
static long textloc_file_mode;

static SPEC_PARAM spec_args[] = {
    {"retrieve.textloc_file",    getspec_dbfile, (char *) &textloc_file},
    {"retrieve.textloc_file.rmode",getspec_filemode, (char *) &textloc_file_mode},
    {"retrieve.doc.index_vec",   getspec_func, (char *) &index_vec},
    TRACE_PARAM ("retrieve.getdoc.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    int textloc_fd;
    PROC_INST index_vec;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;


int
init_getdoc_textloc (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;
    CONTEXT old_context;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_getdoc_textloc");
    
    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];
    
    if (! VALID_FILE (textloc_file)) {
        set_error (SM_ILLPA_ERR, textloc_file, "init_getdoc_textloc");
        return (UNDEF);
    }

    if (UNDEF == (ip->textloc_fd = open_textloc (textloc_file,
                                                 textloc_file_mode)))
        return (UNDEF);

    old_context = get_context();
    set_context (CTXT_DOC);
    /* Call all initialization procedures for indexing */
    ip->index_vec.ptab = index_vec;
    if (UNDEF == (ip->index_vec.inst = index_vec->init_proc (spec, NULL)))
        return (UNDEF);
    set_context (old_context);

    ip->valid_info = 1;
             
    PRINT_TRACE (2, print_string, "Trace: leaving init_getdoc_textloc");
    
    return (new_inst);
}


int
getdoc_textloc (did, vec, inst)
long *did;
VEC *vec;
int inst;
{
    STATIC_INFO *ip;
    int status;
    TEXTLOC textloc;
    EID text_id;

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "getdoc_textloc");
        return (UNDEF);
    }
    ip  = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering getdoc_textloc");

    if (ip->textloc_fd == UNDEF) {
        set_error (SM_INCON_ERR, "Textloc not initialized", "getdoc_textloc");
        return (UNDEF);
    }

    /* If did is non-NULL, then that is the desired document */
    EXT_NONE(text_id.ext);
    if (did != (long *) NULL) {
        text_id.id = *did;

        /* Index the document */
        status = ip->index_vec.ptab->proc (&text_id, vec,
                                                        ip->index_vec.inst);
        if (1 != status)
            return (status);
    } else {
        /* read textloc from textloc_file */
        if (1 != (status = read_textloc (ip->textloc_fd, &textloc)))
            return (status);

        /* Index the textloc document */
        text_id.id = textloc.id_num;
        if (UNDEF == ip->index_vec.ptab->proc (&text_id, vec, ip->index_vec.inst))
            return (UNDEF);
    }

    PRINT_TRACE (4, print_vector, vec);
    PRINT_TRACE (2, print_string, "Trace: leaving getdoc_textloc");
    return (1);
}

int
close_getdoc_textloc(inst)
int inst;
{

    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_getdoc_textloc");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_getdoc_textloc");
        return (UNDEF);
    }

    ip  = &info[inst];
    ip->valid_info--;
    /* Output everything and free buffers if this was last close of this
       inst */
    if (ip->valid_info == 0) {
        if (UNDEF != ip->textloc_fd) {
            if (UNDEF == close_textloc (ip->textloc_fd))
                return (UNDEF);
            ip->textloc_fd = UNDEF;
            if (UNDEF == ip->index_vec.ptab->close_proc (ip->index_vec.inst))
                return (UNDEF);
        }
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_getdoc_textloc");
    return (0);
}
