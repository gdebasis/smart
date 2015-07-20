/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 
 *
 * Generic document expansion flow.
 * 1. Read a direct file (the IN file argument)
 * 2. For each document vector in the direct file
 *    a. Fire this document as a query
 *    b. Get the retrieved results as a tr_vec
 *    c. Re-weight this document vector into an output parameter ovec
 *       by calling doc_exp_func->ptab (a function pointer to the
 *       actual document expansion method)
 * 3. Write the re-weighted vector (ovec) in the OUT file.
 *
   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

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
#include "retrieve.h"

static long in_mode;
static long out_mode;
static PROC_INST docexp_func;
static PROC_INST coll_sim;

static SPEC_PARAM spec_args[] = {
    {"convert.in.rmode",     getspec_filemode,  (char *) &in_mode},
    {"convert.out.rwmode",   getspec_filemode,  (char *) &out_mode},
    {"convert.doc_expansion.weight",   getspec_func,      (char *) &docexp_func.ptab},
    {"retrieve.coll_sim",         getspec_func, (char *) &coll_sim.ptab},
    TRACE_PARAM ("convert.weight_exp.trace")
    };
static int num_spec_args =
    sizeof (spec_args) / sizeof (spec_args[0]);

static SPEC *save_spec;

static int comp_tr_tup_sim (TR_TUP* ptr1, TR_TUP* ptr2);

int init_weight_exp_doc (spec, unused)
SPEC *spec;
char *unused;
{
    PRINT_TRACE (2, print_string, "Trace: entering init_weight_doc");
    
    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);
    save_spec = spec;

    if (UNDEF == (docexp_func.inst = docexp_func.ptab->init_proc (save_spec)))
        return (UNDEF);

    if (UNDEF == (coll_sim.inst = coll_sim.ptab->init_proc (spec, NULL)))
		return UNDEF;

    PRINT_TRACE (2, print_string, "Trace: leaving init_weight_doc");
    return (0);
}        

int
weight_exp_doc (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
    CONTEXT old_context;
    VEC invec, outvec;
    int orig_fd, new_fd;
	RETRIEVAL retrieval;
	RESULT results;
	TR_VEC tr_vec;
	TR_TUP *tr;
	QUERY_VECTOR qvec;
	static char msg[256];

    PRINT_TRACE (2, print_string, "Trace: entering weight_doc");

    old_context = get_context();
    set_context (CTXT_DOC);
    
    /* Call all initialization procedures */
    if (UNDEF == (orig_fd = open_vector (in_file, in_mode)))
        return (UNDEF);
    if (UNDEF == (new_fd = open_vector (out_file,
                                        out_mode|SCREATE)))
        return (UNDEF);
    
	retrieval.tr = &tr_vec;

    /* Get each document in turn */
    while (1 == read_vector (orig_fd, &invec)) {
        qvec.qid = invec.id_num.id;
        qvec.vector = (char *)&invec;

		// Compute the similarity of this current document with the collection.
    	retrieval.query = &qvec;
        results.qid = qvec.qid;
        tr_vec.num_tr = 0;
        if (UNDEF == coll_sim.ptab->proc (&retrieval, &results, coll_sim.inst))
            return (UNDEF);
    	if (UNDEF == res_tr (&results, &tr_vec, 0))
			return (UNDEF);

		snprintf(msg, sizeof(msg), "Trace: Performing document expansion on %d documents", tr_vec.num_tr);
    	PRINT_TRACE (4, print_string, msg);
        if (UNDEF == docexp_func.ptab->proc (&invec, &outvec, &tr_vec, docexp_func.inst)) {
            return (UNDEF);
        }
        if (UNDEF == seek_vector (new_fd, &outvec) ||
            UNDEF == write_vector (new_fd, &outvec)) {
            return (UNDEF);
        }
    }

    /* Close weighting procs */
    if (UNDEF == close_vector (orig_fd) ||
        UNDEF == close_vector (new_fd)) {
        return (UNDEF);
    }

    set_context (old_context);

    PRINT_TRACE (2, print_string, "Trace: leaving weight_doc");

    return (1);
}

int
close_weight_exp_doc (inst)
int inst;
{
    PRINT_TRACE (2,print_string, "Trace: entering/leaving close_weight_doc");
	
    if (UNDEF == docexp_func.ptab->close_proc (docexp_func.inst))
		return UNDEF;
    if (UNDEF == coll_sim.ptab->close_proc (coll_sim.inst))
		return UNDEF;

    return (0);
}

static int comp_tr_tup_sim (TR_TUP* ptr1, TR_TUP* ptr2) {
    if (ptr2->sim > ptr1->sim)
        return 1;
    if (ptr1->sim > ptr2->sim)
        return -1;
    return 0;
}

