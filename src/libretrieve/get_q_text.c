#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libretrieve/get_q_text.c,v 11.2 1993/02/03 16:54:31 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Get the next query from a text file, index it, return result in query_vec
 *1 retrieve.get_query.get_q_text
 *1 retrieve.query_index.std_vec
 *2 get_q_text (unused, query_vec, inst)
 *3   QUERY_VECTOR *query_vec;
 *3   int inst;
 *4 init_get_q_text (spec, unused)
 *5   "index.query.preparse"
 *5   "index.query.next_vecid"
 *5   "index.query.index_pp"
 *5   "retrieve.get_query.trace"
 *4 close_get_q_text (inst)

 *6 global_context is set to indicate indexing query (CTXT_INDEXING_QUERY)

 *7 Get the next query from the text file (retrieve.)query.input_list_file.

 *8 Calls all the indicated procedures to index a query.
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
#include "docindex.h"

/* Get the next query from a text file, then index it and return it */

static PROC_INST pp,             /* Preparse procedures */
    next_vecid,                  /* Determine next valid id to use */
    index_pp;                    /* Indexing procedures */


static SPEC_PARAM spec_args[] = {
    {"index.preparse.query.next_preparse", getspec_func, (char *) &pp.ptab},
    {"index.query.next_vecid",  getspec_func, (char *) &next_vecid.ptab},
    {"index.query.index_pp",    getspec_func, (char *) &index_pp.ptab},
    TRACE_PARAM ("retrieve.get_query.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static VEC vec;

int
init_get_q_text (spec, prefix)
SPEC *spec;
char *prefix;
{
    CONTEXT old_context;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_get_q_text");

    /* Set context indicating that we are indexing a query.  Tells
       inferior procedures to use params appropriate for query instead of
       query */
    old_context = get_context();
    set_context (CTXT_INDEXING_QUERY);

    /* Call all initialization procedures for indexing */
    if (UNDEF == (pp.inst = pp.ptab->init_proc (spec, prefix)) ||
        UNDEF == (next_vecid.inst = next_vecid.ptab->init_proc (spec, NULL)) ||
        UNDEF == (index_pp.inst = index_pp.ptab->init_proc (spec, NULL)))
        return (UNDEF);

    set_context (old_context);
    PRINT_TRACE (2, print_string, "Trace: leaving init_get_q_text");
    return (0);
}

int
get_q_text (unused, query_vec, inst)
char *unused;
QUERY_VECTOR *query_vec;
int inst;
{
    int status;
    SM_INDEX_TEXTDOC pp_vec;

    PRINT_TRACE (2, print_string, "Trace: entering get_q_text");

    /* Get next query id and store in pp_vec */
    if (UNDEF == next_vecid.ptab->proc (NULL, &pp_vec.id_num, next_vecid.inst))
        return (UNDEF);
    
    /* Get next query (if it exists) */
    status =  pp.ptab->proc (NULL, &pp_vec, pp.inst);
    if (status == UNDEF) {
        return (UNDEF);
    }
    if (status == 0) {
        /* End of input */
        return (0);
    }
    if (UNDEF == index_pp.ptab->proc (&pp_vec, &vec, index_pp.inst))
        return (UNDEF);
    
    query_vec->qid = vec.id_num.id;
    query_vec->vector = (char *) &vec;
    
    PRINT_TRACE (2, print_string, "Trace: leaving get_q_text");
    return (1);
}

int
close_get_q_text (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_get_q_text");

    if (UNDEF == pp.ptab->close_proc (pp.inst)||
        UNDEF == next_vecid.ptab->close_proc(next_vecid.inst) ||
        UNDEF == index_pp.ptab->close_proc(index_pp.inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_get_q_text");
    return (0);
}


