#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/weight.c,v 11.2 1993/02/03 16:49:33 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Object level conversion routine to reweight a document vector object
 *1 convert.obj.weight_doc

 *2 weight_doc (in_file, out_file, inst)
 *2   char *in_file;
 *2   char *out_file;
 *2   int inst;

 *4 init_weight_doc (spec, unused)
 *5   "convert.in.rmode"
 *5   "convert.out.rwmode"
 *5   "convert.weight.trace"
 *5   "convert.doc.weight"
 *4 close_weight_doc (inst)

 *6 global_context set to indicate weighting document (CTXT_DOC)
 *7 For every document in in_file, calls weighting procedure given by 
 *7 doc.weight to reweight it, and then write it to out_file.
***********************************************************************/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Object level conversion routine to reweight a query vector object
 *1 convert.obj.weight_query

 *2 weight_query (in_file, out_file, inst)
 *2   char *in_file;
 *2   char *out_file;
 *2   int inst;

 *4 init_weight_query (spec, unused)
 *5   "convert.in.rmode"
 *5   "convert.out.rwmode"
 *5   "convert.weight.trace"
 *5   "convert.query.weight"
 *4 close_weight_query (inst)

 *6 global_context set to indicate weighting query (CTXT_QUERY)
 *7 For every query in in_file, calls weighting procedure given by 
 *7 query.weight to reweight it, and then write it to out_file.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "io.h"
#include "functions.h"
#include "spec.h"
#include "docindex.h"
#include "docdesc.h"
#include "vector.h"
#include "trace.h"
#include "context.h"
#include "vector.h"

/* Convert tf weighted vector file to specified weighting scheme
*/

static long in_mode;
static long out_mode;
static PROC_INST query_func;
static PROC_INST doc_func;

static SPEC_PARAM spec_args[] = {
    {"convert.in.rmode",     getspec_filemode,  (char *) &in_mode},
    {"convert.out.rwmode",   getspec_filemode,  (char *) &out_mode},
    {"convert.query.weight", getspec_func,      (char *) &query_func.ptab},
    {"convert.doc.weight",   getspec_func,      (char *) &doc_func.ptab},
    TRACE_PARAM ("convert.weight.trace")
    };
static int num_spec_args =
    sizeof (spec_args) / sizeof (spec_args[0]);

static SPEC *save_spec;

int
init_weight_doc (spec, unused)
SPEC *spec;
char *unused;
{
    PRINT_TRACE (2, print_string, "Trace: entering init_weight_doc");
    
    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);
    save_spec = spec;

    PRINT_TRACE (2, print_string, "Trace: leaving init_weight_doc");
    return (0);
}        

int
weight_doc (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
    CONTEXT old_context;
    VEC invec, outvec;
    int orig_fd, new_fd;

    PRINT_TRACE (2, print_string, "Trace: entering weight_doc");

    old_context = get_context();
    set_context (CTXT_DOC);
    
    /* Call all initialization procedures */
    if (UNDEF == (doc_func.inst =
                  doc_func.ptab->init_proc (save_spec, "doc.")))
        return (UNDEF);
    if (UNDEF == (orig_fd = open_vector (in_file, in_mode)))
        return (UNDEF);
    if (UNDEF == (new_fd = open_vector (out_file,
                                        out_mode|SCREATE)))
        return (UNDEF);
    
    /* Get each document in turn */
    while (1 == read_vector (orig_fd, &invec)) {
        if (invec.id_num.id < global_start)
            continue;
        if (invec.id_num.id > global_end)
            break;
        if (UNDEF == doc_func.ptab->proc (&invec,
                                            &outvec,
                                            doc_func.inst)) {
            return (UNDEF);
        }
        if (UNDEF == seek_vector (new_fd, &outvec) ||
            UNDEF == write_vector (new_fd, &outvec)) {
            return (UNDEF);
        }
    }

    /* Close weighting procs */
    if (UNDEF == doc_func.ptab->close_proc (doc_func.inst) ||
        UNDEF == close_vector (orig_fd) ||
        UNDEF == close_vector (new_fd)) {
        return (UNDEF);
    }

    set_context (old_context);

    PRINT_TRACE (2, print_string, "Trace: leaving weight_doc");

    return (1);
}

int
close_weight_doc (inst)
int inst;
{
    PRINT_TRACE (2,print_string, "Trace: entering/leaving close_weight_doc");
    return (0);
}


int
init_weight_query (spec, unused)
SPEC *spec;
char *unused;
{
    PRINT_TRACE (2, print_string, "Trace: entering init_weight_query");
    
    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);
    save_spec = spec;

    PRINT_TRACE (2, print_string, "Trace: leaving init_weight_query");
    return (0);
}        


int
weight_query (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
    CONTEXT old_context;
    VEC invec, outvec;
    int orig_fd, new_fd;

    PRINT_TRACE (2, print_string, "Trace: entering weight_query");

    old_context = get_context();
    set_context (CTXT_QUERY);
    
    /* Call all initialization procedures */
    if (UNDEF == (query_func.inst =
                  query_func.ptab->init_proc (save_spec, "query.")))
        return (UNDEF);
    if (UNDEF == (orig_fd = open_vector (in_file, in_mode)))
        return (UNDEF);
    if (UNDEF == (new_fd = open_vector (out_file,
                                        out_mode|SCREATE)))
        return (UNDEF);
    
    /* Get each document in turn */
    while (1 == read_vector (orig_fd, &invec)) {
        if (UNDEF == query_func.ptab->proc (&invec,
                                            &outvec,
                                            query_func.inst)) {
            return (UNDEF);
        }
        if (UNDEF == seek_vector (new_fd, &outvec) ||
            UNDEF == write_vector (new_fd, &outvec)) {
            return (UNDEF);
        }
    }

    /* Close weighting procs */
    if (UNDEF == query_func.ptab->close_proc (query_func.inst) ||
        UNDEF == close_vector (orig_fd) ||
        UNDEF == close_vector (new_fd)) {
        return (UNDEF);
    }

    set_context (old_context);

    PRINT_TRACE (2, print_string, "Trace: leaving weight_query");

    return (1);
}

int
close_weight_query (inst)
int inst;
{
    PRINT_TRACE (2,print_string, "Trace: entering/leaving close_weight_query");
    return (0);
}

