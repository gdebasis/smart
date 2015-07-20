#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libretrieve/get_q_pvec.c,v 11.2 1993/02/03 16:54:34 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Get next pnorm query vector from already indexed query set.
 *1 retrieve.get_query.get_q_pvec

 *2 get_q_pvec (unused, query_vec, inst)
 *3   char *unused;
 *3   QUERY_VECTOR *query_vec;
 *3   int inst;

 *4 init_get_q_pvec (spec, unused)
 *5   "retrieve.query_file"
 *5   "retrieve.query_file.rmode"
 *5   "retrieve.get_query.trace"
 *4 close_get_q_pvec (inst)

 *6 global_start, global_end are used to get range of valid query id's

 *7 Read the next pnorm query vector from query_file. 
 *7 Return 0 if no more queries, UNDEF if error, 1 otherwise.
***********************************************************************/

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
#include "pnorm_vector.h"
#include "inst.h"
#include "local_functions.h"


/* Return the next query from an ordinary vector collection */

static char *query_file;
static long query_file_mode;


static SPEC_PARAM spec_args[] = {
    {"retrieve.query_file",       getspec_dbfile, (char *) &query_file},
    {"retrieve.query_file.rmode", getspec_filemode, (char *) &query_file_mode},
    TRACE_PARAM ("retrieve.get_query.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    int query_fd;
    PNORM_VEC vec;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;


int
init_get_q_pvec (spec, unused)
SPEC *spec;
char *unused;
{
    int new_inst;
    STATIC_INFO *ip;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_get_q_pvec");

    if (! VALID_FILE (query_file)) {
        set_error (SM_ILLPA_ERR, "query_file", "init_get_q_pvec");
        return (UNDEF);
    }

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];

    if (UNDEF == (ip->query_fd = open_pnorm (query_file, query_file_mode)))
        return (UNDEF);

    /* If not starting at beginning of query file, move there */
    if (global_start > 0) {
        ip->vec.id_num.id = global_start; EXT_NONE(ip->vec.id_num.ext);
        if (UNDEF == seek_pnorm (ip->query_fd, &ip->vec))
            return (UNDEF);
    }
        
    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_get_q_pvec");
    return (new_inst);
} /* init_get_q_pvec */


int
get_q_pvec (unused, query_vec, inst)
char *unused;
QUERY_VECTOR *query_vec;
int inst;
{
    STATIC_INFO *ip;
    int status;

    PRINT_TRACE (2, print_string, "Trace: entering get_q_pvec");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "get_q_pvec");
        return (UNDEF);
    }
    ip  = &info[inst];
    
    if (1 == (status = read_pnorm (ip->query_fd, &ip->vec))) {
        if (ip->vec.id_num.id > global_end)
            return (0);
        query_vec->qid = ip->vec.id_num.id;
        query_vec->vector = (char *) &ip->vec;
    }

/*
    PRINT_TRACE (4, print_pnorm_vector, &ip->vec);
*/
    PRINT_TRACE (2, print_string, "Trace: leaving get_q_pvec");
    return (status);
} /* get_q_pvec */


int
close_get_q_pvec (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_get_q_pvec");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_q_vec");
        return (UNDEF);
    }

    ip  = &info[inst];
    ip->valid_info--;

    if (UNDEF == close_pnorm (ip->query_fd))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_get_q_pvec");
    return (0);
} /* close_get_q_pvec */


