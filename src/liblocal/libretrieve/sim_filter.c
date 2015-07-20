#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libretrieve/sim_inner.c,v 11.2 1993/02/03 16:54:43 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Perform innerproduct similarity function between two vectors
 *1 retrieve.ctype_vec.inner
 *2 sim_filter (vec_pair, sim_ptr, inst)
 *3   VEC_PAIR *vec_pair;
 *3   float *sim_ptr;
 *3   int inst;
 *4 init_sim_filter (spec, param_prefix)
 *5   "retrieve.ctype_vec.trace"
 *5   "ctype.*.sim_ctype_weight"
 *4 close_sim_filter (inst)

 *7 Similarity computation for the kind of query formed in liblocal/
 *7 libconvert/vec_filtervec.c
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
#include "inst.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("retrieve.ctype_vec.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static char *prefix;
static float ctype_weight;
static SPEC_PARAM_PREFIX pspec_args[] = {
    {&prefix,  "sim_ctype_weight", getspec_float, (char *) &ctype_weight},
    };
static int num_pspec_args = sizeof (pspec_args) / sizeof (pspec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    float ctype_weight;                /* Weight for this particular ctype */
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static int included(), compare_wt();


int
init_sim_filter (spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    STATIC_INFO *ip;
    int new_inst;

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);
    prefix = param_prefix;
    if (UNDEF == lookup_spec_prefix (spec, &pspec_args[0], num_pspec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_sim_filter");

    ip->ctype_weight = ctype_weight;

    ip->valid_info++;

    PRINT_TRACE (2, print_string, "Trace: leaving init_sim_filter");
    return (new_inst);
}

int
sim_filter (vec_pair, sim_ptr, inst)
VEC_PAIR *vec_pair;
float *sim_ptr;
int inst;
{
    char flag;
    double sim = 0.0;
    CON_WT *cwp, *startp, *endp;
    VEC *v1, *v2;

    PRINT_TRACE (2, print_string, "Trace: entering sim_filter");
    PRINT_TRACE (6, print_vector, vec_pair->vec1);
    PRINT_TRACE (6, print_vector, vec_pair->vec2);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "sim_filter");
        return (UNDEF);
    }

    v1 = vec_pair->vec1; v2 = vec_pair->vec2;
    qsort(v1->con_wtp, v1->num_conwt, sizeof(CON_WT), compare_wt);
    startp = v1->con_wtp; 
    while (startp < v1->con_wtp + v1->num_conwt) {
	endp = startp + 1;
	while (endp < v1->con_wtp + v1->num_conwt &&
	       endp->wt == startp->wt)
	    endp++;
	for (flag = 1, cwp = startp; cwp < endp && flag; cwp++) 
	    if (!included(cwp->con, v2))
		flag = 0;
	if (flag) sim += startp->wt;
	startp = endp;
    }
    *sim_ptr = info[inst].ctype_weight * sim;
    
    PRINT_TRACE (4, print_float, sim_ptr);
    PRINT_TRACE (2, print_string, "Trace: leaving sim_filter");
    return (1);
}


int
close_sim_filter (inst)
int inst;
{
    PRINT_TRACE (2, print_string,
                 "Trace: entering/leaving close_sim_filter");
    return (0);
}

static int
included(con, vec)
long con;
VEC *vec;
{
    CON_WT *cwp;

    for (cwp = vec->con_wtp; 
	 cwp < vec->con_wtp + vec->num_conwt && cwp->con <= con;
	 cwp++)
	if (cwp->con == con) return 1;
    return 0;
}

static int
compare_wt(c1, c2)
CON_WT *c1, *c2;
{
    if (c1->wt > c2->wt)
	return -1;
    if (c1->wt < c2->wt)
	return 1;
    return 0;
}
