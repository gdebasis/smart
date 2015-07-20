#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libretrieve/sim_q_vec.c,v 11.2 1993/02/03 16:54:46 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Calculate similarity between pair of vectors
 *1 retrieve.q_vec.vec_vec
 *2 sim_q_vec (q_vec_pair, q_vec_result, inst)
 *3   Q_VEC_PAIR *q_vec_pair;
 *3   Q_VEC_RESULT *q_vec_result;
 *3   int inst;
 *4 init_sim_q_vec (spec, unused)
 *5   "retrieve.q_vec.trace"
 *5   "index.ctype.*.seq_sim"
 *4 close_sim_q_vec (inst)
 *7 Calculate the similarity between the two vectors (given by q_vec_pair)
 *7 by calling a ctype dependant similarity function given by
 *7 index.ctype.N.seq_sim for each ctype N occurring in the vectors.
 *7 The explanation portion of q_vec_result is presently ignored.

 *9 Note: Assumes similarity is 0.0 between ctypes of the vectors if
 *9 that ctype is not included in one or both of the vectors.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "retrieve.h"
#include "vector.h"
#include "docdesc.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("retrieve.q_vec.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static SM_INDEX_DOCDESC doc_desc;

static int *ctype_inst;

static int num_inst = 0;

int
init_sim_q_vec (spec, unused)
SPEC *spec;
char *unused;
{
    long i;
    char param_prefix[PATH_LEN];

    if (num_inst++ > 0) {
        /* Assume all instantiations equal */
        PRINT_TRACE (2, print_string,
                     "Trace: entering/leaving init_sim_q_vec");
        return (0);
    }

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_sim_q_vec");

    if (UNDEF == lookup_spec_docdesc (spec, &doc_desc))
        return (UNDEF);

    /* Reserve space for inst id of each of the ctype retrieval procs */
    if (NULL == (ctype_inst = (int *)
                 malloc ((unsigned) doc_desc.num_ctypes * sizeof (int))))
        return (UNDEF);
    
    /* Call all initialization procedures */
    for (i = 0; i < doc_desc.num_ctypes; i++) {
        (void) sprintf (param_prefix, "ctype.%ld.", i);
        if (UNDEF == (ctype_inst[i] = doc_desc.ctypes[i].seq_sim->
                      init_proc (spec, param_prefix)))
            return (UNDEF);
    }
    
    PRINT_TRACE (2, print_string, "Trace: leaving init_sim_q_vec");
    return (0);
}

int
sim_q_vec (q_vec_pair, q_vec_result, inst)
Q_VEC_PAIR *q_vec_pair;
Q_VEC_RESULT *q_vec_result;
int inst;
{
    VEC *qvec = (VEC *) q_vec_pair->query_vec->vector;
    VEC *dvec = q_vec_pair->dvec;
    long i;
    VEC ctype_query;
    VEC ctype_doc;
    VEC_PAIR ctype_vec_pair;
    CON_WT *query_con_wtp;
    CON_WT *doc_con_wtp;
    float ctype_sim;

    PRINT_TRACE (2, print_string, "Trace: entering sim_q_vec");
    PRINT_TRACE (6, print_vector, qvec);
    PRINT_TRACE (6, print_vector, dvec);

    /* Perform retrieval, sequentially going through query by ctype */
    ctype_query.id_num = qvec->id_num;
    ctype_query.num_ctype = 1;
    query_con_wtp = qvec->con_wtp;
    ctype_doc.id_num = dvec->id_num;
    ctype_doc.num_ctype = 1;
    doc_con_wtp = dvec->con_wtp;
    ctype_vec_pair.vec1 = &ctype_query;
    ctype_vec_pair.vec2 = &ctype_doc;
    q_vec_result->sim = 0;
    
    for (i = 0; i < doc_desc.num_ctypes; i++) {
	if (qvec->num_ctype <= i || dvec->num_ctype <= i)
	    break;
	if (qvec->ctype_len[i] <= 0 || dvec->ctype_len[i] <= 0) {
	    query_con_wtp += qvec->ctype_len[i];
	    doc_con_wtp += dvec->ctype_len[i];
	    continue;
	}
	
	ctype_query.ctype_len = &qvec->ctype_len[i];
	ctype_query.num_conwt = qvec->ctype_len[i];
	ctype_query.con_wtp = query_con_wtp;
	query_con_wtp += qvec->ctype_len[i];
	ctype_doc.ctype_len = &dvec->ctype_len[i];
	ctype_doc.num_conwt = dvec->ctype_len[i];
	ctype_doc.con_wtp = doc_con_wtp;
	doc_con_wtp += dvec->ctype_len[i];
	if (UNDEF == doc_desc.ctypes[i].seq_sim->
	    proc (&ctype_vec_pair, &ctype_sim, ctype_inst[i]))
	    return (UNDEF);
	q_vec_result->sim += ctype_sim;
    }

    PRINT_TRACE (4, print_float, &q_vec_result->sim);
    PRINT_TRACE (2, print_string, "Trace: leaving sim_q_vec");
    return (1);
}


int
close_sim_q_vec (inst)
int inst;
{
    long i;
 
    if (--num_inst) {
        PRINT_TRACE (2, print_string,
                     "Trace: entering/leaving close_sim_q_vec");
        return (0);
    }

    PRINT_TRACE (2, print_string, "Trace: entering close_sim_q_vec");

    for (i = 0; i < doc_desc.num_ctypes; i++) {
        if (UNDEF == (doc_desc.ctypes[i].seq_sim->close_proc (ctype_inst[i])))
            return (UNDEF);
    }

    (void) free ((char *) ctype_inst);

    PRINT_TRACE (2, print_string, "Trace: leaving close_sim_q_vec");
    return (0);
}
