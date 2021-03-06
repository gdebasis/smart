#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/index_vec.c,v 11.2 1993/02/03 16:50:53 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 index a document vector given its text location
 *1 index.vec_index.doc
 *2 index_doc (text_id, vec, inst)
 *3   EID *text_id;
 *3   VEC *vec;
 *3   int inst;

 *4 init_index_doc (spec, qd_prefix)
 *5   "index.preparse.doc.named_preparse"
 *5   "index.doc.index_pp"
 *5   "index.vector.trace"
 *4 close_index_doc (inst)

 *6 global_context temporarily set to indicate CTXT_DOC

 *7 Get a preparsed text (via preparse) from the text indicated by text_id.
 *7 Then index that preparsed text obtaining a vector vec using the
 *7 procedure indicated by index_pp.
 *7 Return UNDEF if error, else 1
***********************************************************************/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 index a query vector given its text location
 *1 index.vec_index.query
 *2 index_query (text_id, vec, inst)
 *3   EID *text_id;
 *3   VEC *vec;
 *3   int inst;

 *4 init_index_query (spec, qd_prefix)
 *5   "index.preparse.query.named_preparse"
 *5   "index.query.index_pp"
 *5   "index.vector.trace"
 *4 close_index_query (inst)

 *6 global_context temporarily set to indicate CTXT_QUERY

 *7 Get a preparsed text (via preparse) from the text indicated by text_id.
 *7 Then index that preparsed text obtaining a vector vec using the
 *7 procedure indicated by index_pp.
 *7 Return UNDEF if error, else 1
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "docindex.h"
#include "vector.h"
#include "trace.h"
#include "context.h"
#include "inst.h"

static PROC_TAB *pp,               /* Preparse procedures */
    *post_pp,			   /* post processing procedure */
    *index_pp;                     /* Indexing procedure */

static SPEC_PARAM doc_spec_args[] = {
    {"index.preparse.doc.named_preparse", getspec_func, (char *) &pp},
    {"index.doc.post_pp", getspec_func, (char *) &post_pp},
    {"index.doc.index_pp",                getspec_func, (char *) &index_pp},
    TRACE_PARAM ("index.vector.trace")
    };
static int num_doc_spec_args = sizeof (doc_spec_args) /
         sizeof (doc_spec_args[0]);

static SPEC_PARAM query_spec_args[] = {
    {"index.preparse.query.named_preparse", getspec_func, (char *) &pp},
    {"index.query.index_pp",                getspec_func, (char *) &index_pp},
    TRACE_PARAM ("index.vector.trace")
    };
static int num_query_spec_args = sizeof (query_spec_args) /
         sizeof (query_spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    PROC_INST pp;
    PROC_INST post_pp;
    PROC_INST index_pp;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_index_query (spec, qd_prefix)
SPEC *spec;
char *qd_prefix;
{
    CONTEXT old_context;
    int new_inst;
    STATIC_INFO *ip;

    /* Set context indicating that we are indexing a query.  Tells
       inferior procedures to use params appropriate for query instead of
       doc */
    old_context = get_context();
    set_context (CTXT_QUERY);
    if (UNDEF == lookup_spec (spec,
                              &query_spec_args[0],
                              num_query_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_index_query");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];

    /* Call all initialization procedures */
    ip->pp.ptab = pp;
    ip->index_pp.ptab = index_pp;
    if (UNDEF == (ip->pp.inst = ip->pp.ptab->init_proc (spec, qd_prefix)) ||
        UNDEF == (ip->index_pp.inst = 
                  ip->index_pp.ptab->init_proc (spec, qd_prefix)))
        return (UNDEF);

    ip->valid_info = 1;
    set_context (old_context);

    PRINT_TRACE (2, print_string, "Trace: leaving init_index_query");

    return (new_inst);
}

int
index_query (text_id, vec, inst)
EID *text_id;
VEC *vec;
int inst;
{
    SM_INDEX_TEXTDOC pp_vec;
    STATIC_INFO *ip;
    int status;

    PRINT_TRACE (2, print_string, "Trace: entering index_query");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "index_query");
        return (UNDEF);
    }
    ip  = &info[inst];

    pp_vec.id_num = *text_id;

    status = ip->pp.ptab->proc (text_id, &pp_vec, ip->pp.inst);
    if (1 != status)
	return (status);
    if (UNDEF == ip->index_pp.ptab->proc (&pp_vec, vec, ip->index_pp.inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving index_query");

    return (1);
}

int
close_index_query (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_index_query");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_index_query");
        return (UNDEF);
    }
    
    ip  = &info[inst];
    ip->valid_info--;

    if (UNDEF == ip->pp.ptab->close_proc (ip->pp.inst)||
        UNDEF == ip->index_pp.ptab->close_proc (ip->index_pp.inst))
        return (UNDEF);
    
    PRINT_TRACE (2, print_string, "Trace: leaving close_index_query");
    return (0);
}


int
init_index_doc (spec, qd_prefix)
SPEC *spec;
char *qd_prefix;
{
    CONTEXT old_context;
    int new_inst;
    STATIC_INFO *ip;

    /* Set context indicating that we are indexing a doc.  Tells
       inferior procedures to use params appropriate for doc instead of
       query */
    old_context = get_context();
    set_context (CTXT_DOC);
    if (UNDEF == lookup_spec (spec,
                              &doc_spec_args[0],
                              num_doc_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_index_doc");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];

    /* Call all initialization procedures */
    ip->pp.ptab = pp;
    ip->post_pp.ptab = post_pp;
    ip->index_pp.ptab = index_pp;
    if (UNDEF == (ip->pp.inst =
                  ip->pp.ptab->init_proc (spec, qd_prefix)) ||
	UNDEF == (ip->post_pp.inst = 
		  ip->post_pp.ptab->init_proc (spec, qd_prefix)) ||
	UNDEF == (ip->index_pp.inst = 
                  ip->index_pp.ptab->init_proc (spec, qd_prefix)))
        return (UNDEF);

    ip->valid_info = 1;
    set_context (old_context);

    PRINT_TRACE (2, print_string, "Trace: leaving init_index_doc");

    return (new_inst);
}

int
index_doc (text_id, vec, inst)
EID *text_id;
VEC *vec;
int inst;
{
    SM_INDEX_TEXTDOC pp_vec;
    STATIC_INFO *ip;
    int status;

    PRINT_TRACE (2, print_string, "Trace: entering index_doc");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "index_doc");
        return (UNDEF);
    }
    ip  = &info[inst];

    pp_vec.id_num = *text_id;

    status = ip->pp.ptab->proc (text_id, &pp_vec, ip->pp.inst);
    if (1 != status)
	return (status);
    if (UNDEF == ip->post_pp.ptab->proc (&pp_vec, (SM_INDEX_TEXTDOC *) NULL, 
					 ip->post_pp.inst) ||
	UNDEF == ip->index_pp.ptab->proc (&pp_vec, vec, ip->index_pp.inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "leaving: entering index_doc");

    return (1);
}

int
close_index_doc (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_index_doc");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_index_doc");
        return (UNDEF);
    }
    
    ip  = &info[inst];
    ip->valid_info--;

    if (UNDEF == ip->pp.ptab->close_proc (ip->pp.inst)||
        UNDEF == ip->post_pp.ptab->close_proc (ip->post_pp.inst) ||
        UNDEF == ip->index_pp.ptab->close_proc (ip->index_pp.inst))
        return (UNDEF);
    
    PRINT_TRACE (2, print_string, "Trace: leaving close_index_doc");
    return (0);
}
