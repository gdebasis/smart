#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libretrieve/get_seen.c,v 11.2 1993/02/03 16:54:35 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Get list of already seen docs for this query.
 *1 retrieve.get_seen.get_seen
 *2 get_seen_docs (query_vec, tr_vec, inst)
 *3   QUERY_VECTOR *query_vec;
 *3   TR_VEC *tr_vec;
 *3   int inst;
 *4 init_get_seen_docs (spec, unused)
 *5   "get_seen.tr_file"
 *5   "get_seen.tr_file.rmode"
 *5   "retrieve.get_seen.trace"
 *4 close_get_seen_docs(inst)

 *7 Return in tr_vec a list of documents that are not desired to be
 *7 retrieved for the query given by query_vec->qid.  These docs are
 *7 presumably not desired because the user has already seen them in a
 *7 previous retrieval run.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "io.h"
#include "inst.h"
#include "tr_vec.h"
#include "retrieve.h"


static char *tr_file;     
static long tr_mode;

static SPEC_PARAM spec_args[] = {
    {"get_seen.tr_file",       getspec_dbfile,      (char *) &tr_file},
    {"get_seen.tr_file.rmode", getspec_filemode,    (char *) &tr_mode},
    TRACE_PARAM ("retrieve.get_seen.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    int fd;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_get_seen_docs (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;

    if (UNDEF == lookup_spec(spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_get_seen_docs");
    
    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];

    ip->fd = UNDEF;
    if (VALID_FILE (tr_file) && 
        UNDEF == (ip->fd = open_tr_vec (tr_file, tr_mode)))
        return (UNDEF);

    ip->valid_info = 1;
             
    PRINT_TRACE (2, print_string, "Trace: leaving init_get_seen_docs");

    return (new_inst);
}

int
get_seen_docs (query_vec, tr_vec, inst)
QUERY_VECTOR *query_vec;
TR_VEC *tr_vec;
int inst;
{
    STATIC_INFO *ip;
    int status;

    PRINT_TRACE (2, print_string, "Trace: entering get_seen_docs");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "get_seen_docs");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (ip->fd == UNDEF) {
        PRINT_TRACE (2, print_string, "Trace: leaving get_seen_docs");
        return (0);
    }
    tr_vec->qid = query_vec->qid;
    tr_vec->num_tr = 0;
    if (1 == (status = seek_tr_vec (ip->fd, tr_vec))) {
        if (1 != read_tr_vec (ip->fd, tr_vec))
            return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving get_seen_docs");

    return (status);
}

int
close_get_seen_docs(inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_get_seen_docs");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_get_seen_docs");
        return (UNDEF);
    }

    ip  = &info[inst];
    ip->valid_info--;
    /* Output everything and free buffers if this was last close of this
       inst */
    if (ip->valid_info == 0) {
        if (UNDEF == close_tr_vec (ip->fd))
            return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_get_seen_docs");
    return (0);
}

