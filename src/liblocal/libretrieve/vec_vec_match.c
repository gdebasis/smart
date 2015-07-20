#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libretrieve/vec_vec_match.c,v 11.2 1993/02/03 16:54:46 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Calculate similarity between pair of vectors
 *1 local.retrieve.vec_vec.match
 *2 vec_vec_match (vec_pair, sim_ptr, inst)
 *3   VEC_PAIR *vec_pair;
 *3   float *sim_ptr;
 *3   int inst;
 *4 init_vec_vec_match (spec, unused)
 *5   "retrieve.vec_vec.trace"
 *5   "index.ctype.*.seq_sim"
 *4 close_vec_vec_match (inst)
 *7 Calculate the similarity between the two vectors (given by vec_pair)
 *7 by calling a ctype dependant similarity function given by
 *7 index.ctype.N.seq_sim for each ctype N occurring in the vectors.
 *7 Incorporates run-time normalization.
 *7 For use of major_ctype, see liblocal/libconvert/weight_unique.c
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

static long major_ctype;
static float pivot;
static float inv_slope;

static SPEC_PARAM spec_args[] = {
    {"local.retrieve.vec_vec.major_ctype", getspec_long, (char *) &major_ctype},
    {"local.retrieve.vec_vec.pivot", getspec_float, (char *) &pivot},
    {"local.retrieve.vec_vec.inv_slope", getspec_float, (char *) &inv_slope},

    TRACE_PARAM ("local.retrieve.vec_vec.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static SM_INDEX_DOCDESC doc_desc;

static int *ctype_inst;

static int num_inst = 0;

int
init_vec_vec_match (spec, unused)
SPEC *spec;
char *unused;
{
    long i;
    char param_prefix[PATH_LEN];

    if (num_inst++ > 0) {
        /* Assume all instantiations equal */
        PRINT_TRACE (2, print_string,
                     "Trace: entering/leaving init_vec_vec_match");
        return (0);
    }

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_vec_vec_match");

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
    
    PRINT_TRACE (2, print_string, "Trace: leaving init_vec_vec_match");
    return (0);
}

int
vec_vec_match (vec_pair, sim_ptr, inst)
VEC_PAIR *vec_pair;
float *sim_ptr;
int inst;
{
    VEC *qvec = vec_pair->vec1;
    VEC *dvec = vec_pair->vec2;
    long i;
    VEC ctype_query;
    VEC ctype_doc;
    VEC_PAIR ctype_vec_pair;
    CON_WT *query_con_wtp;
    CON_WT *doc_con_wtp;
    float ctype_sim;
    long num_uniq;
    float total_qwt = 0.0, max_qwt = 0.0;

    PRINT_TRACE (2, print_string, "Trace: entering vec_vec_match");
    PRINT_TRACE (6, print_vector, qvec);
    PRINT_TRACE (6, print_vector, dvec);

    /* Optimize case where both doc and query have one ctype */
    if (qvec->num_ctype == 1 && dvec->num_ctype == 1) {
        if (UNDEF == doc_desc.ctypes[0].seq_sim->
            proc (vec_pair, sim_ptr, ctype_inst[0]))
            return (UNDEF);
    }
    else {
        /* Perform retrieval, sequentially going through query by ctype */
        ctype_query.id_num = qvec->id_num;
        ctype_query.num_ctype = 1;
        query_con_wtp = qvec->con_wtp;
        ctype_doc.id_num = dvec->id_num;
        ctype_doc.num_ctype = 1;
        doc_con_wtp = dvec->con_wtp;
        ctype_vec_pair.vec1 = &ctype_query;
        ctype_vec_pair.vec2 = &ctype_doc;
        *sim_ptr = 0.0;
        
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
            *sim_ptr += ctype_sim;
        }
    }

    if (-1 == major_ctype) {
        num_uniq = dvec->num_conwt;
	for (i = 0; i < qvec->num_conwt; i++) {
	    total_qwt += qvec->con_wtp[i].wt;
	    if (max_qwt < qvec->con_wtp[i].wt) 
		max_qwt = qvec->con_wtp[i].wt;
	}
    }
    else if (major_ctype < dvec->num_ctype &&
	     major_ctype < qvec->num_ctype) {
	num_uniq = dvec->ctype_len[major_ctype];
	query_con_wtp = qvec->con_wtp;
	for (i = 0; i < major_ctype; i++)
	    query_con_wtp += qvec->ctype_len[i];
	for (i = 0; i < qvec->ctype_len[major_ctype]; i++) {
	    total_qwt += query_con_wtp[i].wt;
	    if (max_qwt < query_con_wtp[i].wt) 
		max_qwt = query_con_wtp[i].wt;
	}
    }
    else {
        set_error (SM_INCON_ERR, "Major_ctype is too large", "weight_unique");
        return (UNDEF);
    }

    /* if max_qwt > 0 then so is total_qwt */
    assert(inv_slope> 0 && pivot > 0 && num_uniq > 0 && max_qwt > 0);
    *sim_ptr /= 1.0 + (1.0/inv_slope)*(num_uniq/pivot)*(total_qwt/max_qwt);

    PRINT_TRACE (4, print_float, sim_ptr);
    PRINT_TRACE (2, print_string, "Trace: leaving vec_vec_match");
    return (1);
}


int
close_vec_vec_match (inst)
int inst;
{
    long i;
 
    if (--num_inst) {
        PRINT_TRACE (2, print_string,
                     "Trace: entering/leaving close_vec_vec_match");
        return (0);
    }

    PRINT_TRACE (2, print_string, "Trace: entering close_vec_vec_match");

    for (i = 0; i < doc_desc.num_ctypes; i++) {
        if (UNDEF == (doc_desc.ctypes[i].seq_sim->close_proc (ctype_inst[i])))
            return (UNDEF);
    }

    (void) free ((char *) ctype_inst);

    PRINT_TRACE (2, print_string, "Trace: leaving close_vec_vec_match");
    return (0);
}
