#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libretrieve/q_tr_wtdoc.c,v 11.2 1993/02/03 16:52:42 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Compute similarities between a query and a given set of docs
 *1 local.retrieve.q_tr.wtdoc
 *2 q_tr_wtdoc (in_tr, out_res, inst)
 *3 TR_VEC *in_tr;
 *3 RESULT *out_res;
 *3 int inst;
 *4 init_q_tr_wtdoc (spec, unused)
 *4 close_q_tr_wtdoc (inst)
 *7 For the documents in seen_vec, get the weighted vector and
 *7 recompute similarity to the query.
 *9
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "vector.h"
#include "tr_vec.h"
#include "retrieve.h"

static long top_reranked, top_saved;
static float coeff;
static PROC_TAB *vec_vec;
static SPEC_PARAM spec_args[] = {
    {"retrieve.q_tr.top_reranked", getspec_long, (char *) &top_reranked},
    {"retrieve.q_tr.top_saved", getspec_long, (char *) &top_saved},
    {"retrieve.q_tr.factor", getspec_float, (char *) &coeff},
    {"retrieve.q_tr.vec_vec", getspec_func, (char *) &vec_vec},
    TRACE_PARAM ("retrieve.q_tr.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static long max_tr_buf;
static TOP_RESULT *tr_buf;
static int qid_vec_inst, did_vec_inst, vec_vec_inst;

static int comp_rank(), comp_sim();


int
init_q_tr_wtdoc (spec, unused)
SPEC *spec;
char *unused;
{
    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);
    PRINT_TRACE (2, print_string, "Trace: entering init_q_tr_wtdoc");

    /* Call all initialization procedures */
    if (UNDEF == (qid_vec_inst = init_qid_vec (spec, (char *) NULL)) ||
	UNDEF == (did_vec_inst = init_did_vec (spec, (char *) NULL)) ||
	UNDEF == (vec_vec_inst = vec_vec->init_proc(spec, (char *) NULL)))
	return UNDEF;

    max_tr_buf = top_reranked;
    if (NULL == (tr_buf = Malloc(top_reranked, TOP_RESULT)))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_q_tr_wtdoc");
    return (0);
}

int
q_tr_wtdoc (in_tr, out_res, inst)
TR_VEC *in_tr;
RESULT *out_res;
int inst;
{
    int i;
    float newsim;
    VEC	qvec, dvec;
    VEC_PAIR pair;

    PRINT_TRACE (2, print_string, "Trace: entering q_tr_wtdoc");
    PRINT_TRACE (6, print_tr_vec, in_tr);

    /* Get query vector */
    qvec.id_num.id = in_tr->qid; EXT_NONE(qvec.id_num.ext);
    if (UNDEF == qid_vec(&(qvec.id_num.id), &qvec, qid_vec_inst))
	return(UNDEF);
    PRINT_TRACE (4, print_vector, &qvec);
    pair.vec1 = &qvec;
    pair.vec2 = &dvec;

    qsort((char *) in_tr->tr, in_tr->num_tr, sizeof(TR_TUP), comp_rank);
    for (i=0; i < top_reranked; i++) {
	newsim = 0;
	dvec.id_num.id = in_tr->tr[i].did; EXT_NONE(dvec.id_num.ext);
	if (UNDEF == did_vec(&(dvec.id_num.id), &dvec, did_vec_inst) ||
	    UNDEF == vec_vec->proc(&pair, &newsim, vec_vec_inst))
	    return(UNDEF);
        PRINT_TRACE (4, print_vector, &dvec);
	in_tr->tr[i].sim += coeff * newsim;
    }
    qsort((char *) in_tr->tr, in_tr->num_tr, sizeof(TR_TUP), comp_sim);

    if (in_tr->num_tr > top_saved) in_tr->num_tr = top_saved;
    if (in_tr->num_tr > max_tr_buf) {
	Free(tr_buf);
	max_tr_buf += in_tr->num_tr;
	if (NULL == (tr_buf = Malloc(max_tr_buf, TOP_RESULT)))
	    return(UNDEF);
    }
    for (i = 0; i < in_tr->num_tr; i++) {
	tr_buf[i].did = in_tr->tr[i].did;
	tr_buf[i].sim = in_tr->tr[i].sim;
    }
    out_res->qid = in_tr->qid;
    out_res->top_results = tr_buf;
    out_res->num_top_results = in_tr->num_tr;
    /* the remaining fields are hopefully not used */
    out_res->num_full_results = 0;
    out_res->min_top_sim = UNDEF;

    PRINT_TRACE (4, print_top_results, out_res);
    PRINT_TRACE (2, print_string, "Trace: leaving q_tr_wtdoc");
    return (1);
}


int
close_q_tr_wtdoc (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_q_tr_wtdoc");

    if (UNDEF == close_qid_vec(qid_vec_inst) ||
	UNDEF == close_did_vec(did_vec_inst) ||
	UNDEF == vec_vec->close_proc(vec_vec_inst))
	return UNDEF;

    Free(tr_buf);

    PRINT_TRACE (2, print_string, "Trace: leaving close_q_tr_wtdoc");
    return (0);
}

static int 
comp_rank (tr1, tr2)
TR_TUP *tr1;
TR_TUP *tr2;
{
    if (tr1->rank > tr2->rank) return 1;
    if (tr1->rank < tr2->rank) return -1;

    return 0;
}

static int 
comp_sim (tr1, tr2)
TR_TUP *tr1;
TR_TUP *tr2;
{
    if (tr1->sim > tr2->sim) return -1;
    if (tr1->sim < tr2->sim) return 1;

    if (tr1->did > tr2->did) return 1;
    if (tr1->did < tr2->did) return -1;

    return 0;
}
