#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libretrieve/vecs_vecs.c,v 11.2 1993/02/03 16:54:48 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Compute pairwise similarities between two lists of vectors
 *1 retrieve.vecs_vecs.vecs_vecs
 *2 vecs_vecs (vec_list_pair, results , inst)
 *3   VEC_LIST_PAIR *vec_list_pair;
 *3   ALT_RESULTS *results;
 *3   int inst;
 *4 init_vecs_vecs (spec, unused)
 *5   "retrieve.vecs_vecs.vec_vec"
 *5   "retrieve.vecs_vecs.trace"
 *4 close_vecs_vecs (inst)
 *7 Each vector in vec_list_pair->vec_list1 is compared against every
 *7 vector in vec_list_pair->vec_list2.  All non-zero similarities are
 *7 returned in results.  The comparison between vectors is done by the
 *7 procedure given by vec_vec.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "inst.h"
#include "vector.h"
#include "retrieve.h"

/* Sequentially compare a group of vecs against another group of vecs,
   reporting all non_zero similarities */

static PROC_TAB *vec_vec_ptab;  /* Procedure to calc sim for individual
                                 * vector against individual vector */

static SPEC_PARAM spec_args[] = {
    {"retrieve.vecs_vecs.vec_vec", getspec_func, (char *) &vec_vec_ptab},
    TRACE_PARAM ("retrieve.vecs_vecs.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

typedef struct {
    int valid_info;	/* bookkeeping */

    PROC_INST vec_vec;
    long num_result_space;
    RESULT_TUPLE *result_buf;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_vecs_vecs (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_vecs_vecs");

    NEW_INST( new_inst );
    if (UNDEF == new_inst)
	return UNDEF;
    ip = &info[new_inst];
    ip->valid_info = 1;

    /* Call all initialization procedures */
    ip->vec_vec.ptab = vec_vec_ptab;
    if (UNDEF==(ip->vec_vec.inst =ip->vec_vec.ptab->init_proc (spec, NULL))) {
        return (UNDEF);
    }

    ip->num_result_space = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving init_vecs_vecs");
    return new_inst;
}

int
vecs_vecs (vec_list_pair, results , inst)
VEC_LIST_PAIR *vec_list_pair;
ALT_RESULTS *results;
int inst;
{
    STATIC_INFO *ip;
    float sim;
    VEC *vec1, *vec2;
    VEC_PAIR vec_pair;

    PRINT_TRACE( 2, print_string, "Trace: entering vecs_vecs" );
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "vecs_vecs");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Reserve space for results */
    if (vec_list_pair->vec_list1->num_vec *
        vec_list_pair->vec_list2->num_vec > ip->num_result_space) {
        if (ip->num_result_space != 0)
            Free ( ip->result_buf);
        ip->num_result_space = vec_list_pair->vec_list1->num_vec *
            vec_list_pair->vec_list2->num_vec;
        if (NULL==(ip->result_buf=Malloc(ip->num_result_space,RESULT_TUPLE)))
            return (UNDEF);
    }

    results->results = ip->result_buf;
    results->num_results = 0;

    /* Compare list1 against list2 using sim measure add */
    for (vec1 = vec_list_pair->vec_list1->vec;
         vec1 < &vec_list_pair->vec_list1->vec[vec_list_pair->vec_list1->num_vec];
         vec1++) {
        if (vec1->num_conwt == 0)
            continue;
        for (vec2 = vec_list_pair->vec_list2->vec;
             vec2 < &vec_list_pair->vec_list2->vec[vec_list_pair->vec_list2->num_vec];
             vec2++) {
            if (vec2->num_conwt == 0)
                continue;
            vec_pair.vec1 = vec1;
            vec_pair.vec2 = vec2;
            if (UNDEF == ip->vec_vec.ptab->proc (&vec_pair, &sim,
						 ip->vec_vec.inst))
                return (UNDEF);
            if (sim > 0.0) {
                ip->result_buf[results->num_results].qid = vec1->id_num;
                ip->result_buf[results->num_results].did = vec2->id_num;
                ip->result_buf[results->num_results].sim = sim;
                results->num_results++;
            }
        }
    }

    return (1);
}


int
close_vecs_vecs (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_vecs_vecs");
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_locvec_to_veclist");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (ip->num_result_space > 0)
        Free (ip->result_buf);
    ip->num_result_space = 0;
    if (UNDEF == ip->vec_vec.ptab->close_proc (ip->vec_vec.inst))
        return (UNDEF);

    ip->valid_info = 0;
    PRINT_TRACE (2, print_string, "Trace: leaving close_vecs_vecs");
    return (0);
}

