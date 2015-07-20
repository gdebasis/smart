#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libretrieve/sim_vec_vec.c,v 11.2 1993/02/03 16:54:46 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Calculate similarity between pair of pnorm vectors
 *1 
 *2 sim_pvec_pvec (vec_pair, sim_ptr, inst)
 *3   PVEC_VEC_PAIR *vec_pair;
 *3   float *sim_ptr;
 *3   int inst;
 *4 init_sim_pvec_pvec (spec, unused)
 *4 close_sim_pvec_pvec (inst)
 *7 Calculate the similarity between two pnorm vectors.
 *9 Handles ctype 0 only.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "local_functions.h"
#include "pnorm_common.h"
#include "pnorm_vector.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("retrieve.pvec_pvec.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

double eval_p_formula(), robust_pow();

static int pvec_pvec();
static float get_docwt();


int
init_sim_pvec_pvec (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering/leaving init_sim_pvec_pvec");
    return (0);
}

int
sim_pvec_pvec (vec_pair, sim_ptr, inst)
PVEC_VEC_PAIR *vec_pair;
float *sim_ptr;
int inst;
{

    PRINT_TRACE (2, print_string, "Trace: entering sim_pvec_pvec");

    if (UNDEF == pvec_pvec(vec_pair, sim_ptr, 0))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving sim_pvec_pvec");
    return (1);
}

int
close_sim_pvec_pvec (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering/leaving close_sim_pvec_pvec");
    return (0);
}


static int
pvec_pvec (vec_pair, sim_ptr, root)
PVEC_VEC_PAIR *vec_pair;
float *sim_ptr;
int root;
{
    long current_con, child, index;
    float child_sim;
    double d_wt, q_wt, numerator, denominator, power;
    TREE_NODE *tree_ptr;
    OP_P_WT *op_ptr;
    PNORM_VEC *query = vec_pair->qvec;
    VEC *doc = vec_pair->dvec;

    PRINT_TRACE (6, print_pnorm_vector, query);
    PRINT_TRACE (6, print_pnorm_vector, doc);

    tree_ptr = query->tree + root;
    index = tree_ptr->info;

    /* Tree is single term */
    if (UNDEF == tree_ptr->child) {   
	current_con = (query->con_wtp + index)->con;
	*sim_ptr = get_docwt(doc, current_con);
	return(1);
    }

    /* Processing for operators */
    op_ptr = query->op_p_wtp + index;

    if (op_ptr->op == NOT_OP) {
	if (UNDEF == pvec_pvec(vec_pair, &child_sim, tree_ptr->child))
	    return(UNDEF);
	*sim_ptr = 1 - child_sim;
	return(1);
    } /* end of processing NOT */

    denominator = 0.0; numerator = 0.0;
    /* AND or OR operators: process each child */
    for (child = tree_ptr->child; child != UNDEF;
	 child = (query->tree + child)->sibling) {

	if (UNDEF == pvec_pvec(vec_pair, &child_sim, child))
	    return(UNDEF);

	/* Store the child's weight and its weight raised to the power p */
	if ((query->tree + child)->child == UNDEF)
	    q_wt = (query->con_wtp + (query->tree+child)->info)->wt;
	else /* child is an operator node */
	    q_wt = (query->op_p_wtp + (query->tree+child)->info)->wt;
	if (op_ptr->p != P_INFINITY) {
	    if (FUNDEF == (power = robust_pow(q_wt, (double)op_ptr->p)))
		return(UNDEF);
	    denominator += power;
	}
	else
	    denominator = MAX(denominator, q_wt);

	if (op_ptr->op == AND_OP)
	    d_wt = 1.0 - child_sim;
	else /* op is OR */
	    d_wt = child_sim;

	if (op_ptr->p != P_INFINITY) {
	    if (FUNDEF == (power = robust_pow(q_wt * d_wt, (double)op_ptr->p)))
		return(UNDEF);
	    numerator += power;
	}
	else
	    numerator = MAX(numerator, d_wt*q_wt);
    }  /* end of for */

    *sim_ptr = eval_p_formula(op_ptr->p, op_ptr->op, numerator, denominator);

    PRINT_TRACE (4, print_float, sim_ptr);
    return(1);
}

static float
get_docwt(dvec, con)
VEC *dvec;
long con;
{
    long i;

    for (i = 0; i < dvec->ctype_len[0]; i++)
	if (con == dvec->con_wtp[i].con)
	    return(dvec->con_wtp[i].wt);
    return(0.0);
}
