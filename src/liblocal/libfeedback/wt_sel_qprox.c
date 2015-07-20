#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libfeedback/wt_fdbk_sel_qprox.c,v 11.2 1993/02/03 16:50:03 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "feedback.h"


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Low-level rocchio feedback weighting procedure
 *1 local.feedback.weight.fdbk_sel
 *2 wt_fdbk_sel_qprox (unused1, fdbk_info, inst)
 *3   char *unused1;
 *3   FEEDBACK_INFO *fdbk_info;
 *3   int inst;
 *4 init_wt_fdbk_sel_qprox (spec, unused)
 *5   "feedback.alpha"
 *5   "feedback.beta"
 *5   "feedback.gamma"
 *5   "feedback.weight.trace"
 *4 close_wt_fdbk_sel_qprox (inst)
 *7 Same as feedback.weight.fdbk (libfeedback/wt_fdbk.c), except that
 *7 selection of ctype dependent num_expand terms occurs here. 
 *7 Term selection: all query terms selected; ctype 0 expansion terms 
 *7	selected based on proximity weights.
 *7 All terms weighted using Rocchio.
***********************************************************************/

static float alpha;
static float beta;
static float gammavar;
static long num_ctypes;

static SPEC_PARAM spec_args[] = {
    {"feedback.alpha",		getspec_float, (char *) &alpha},
    {"feedback.beta",		getspec_float, (char *) &beta},
    {"feedback.gamma",		getspec_float, (char *) &gammavar},
    {"feedback.num_ctypes",	getspec_long, (char *) &num_ctypes},
    TRACE_PARAM ("feedback.weight.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static char *prefix;
static long num_expand; /* Number of terms to add */
static SPEC_PARAM_PREFIX prefix_spec_args[] = {
    {&prefix, "num_expand",       getspec_long, (char *) &num_expand}
};
static int num_prefix_spec_args =
    sizeof (prefix_spec_args) / sizeof (prefix_spec_args[0]);

static SPEC *saved_spec;

static int compare_con(), compare_q_wt();

int
init_wt_fdbk_sel_qprox (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_wt_fdbk_sel_qprox");

    saved_spec = spec;

    PRINT_TRACE (2, print_string, "Trace: leaving init_wt_fdbk_sel_qprox");
    return (1);
}

int
wt_fdbk_sel_qprox (unused, fdbk_info, inst)
char *unused;
FEEDBACK_INFO *fdbk_info;
int inst;
{
    char param_prefix[PATH_LEN];
    long num_so_far, i, j;
    double rel_wt, nrel_wt, inv_rel, inv_nrel, sum_old, sum_new;
    OCC *start_ctype, *end_occ, *occp, *prox_occp;

    PRINT_TRACE (2, print_string, "Trace: entering wt_fdbk_sel_qprox");

    inv_rel = (fdbk_info->num_rel > 0) ? beta / fdbk_info->num_rel : 0.0;
    inv_nrel = (fdbk_info->tr->num_tr > fdbk_info->num_rel) ? 
	gammavar / (fdbk_info->tr->num_tr - fdbk_info->num_rel) : 0.0;

    /* Ctype 0 terms are selected by proximity weights. Proximity weights 
     * for terms are stored in the ctype 2 component of fdbk_info. So,
     * for ctype 0, weight is filled in from there, and for ctype 1, weight
     * is calculated as usual.
     */
    start_ctype = occp = fdbk_info->occ;
    end_occ = fdbk_info->occ + fdbk_info->num_occ;
    prox_occp = fdbk_info->occ;
    while (prox_occp < end_occ && prox_occp->ctype <= 1)
	prox_occp++;
    while (occp < end_occ && occp->ctype == 0 && prox_occp < end_occ) {
	if (prox_occp->con > occp->con) occp++;
	else if (prox_occp->con < occp->con) prox_occp++;
	else {
	    occp->weight = prox_occp->rel_weight;
	    occp++; prox_occp++;
	}
    }
    while (occp < end_occ && occp->ctype == 1) {
	rel_wt = occp->rel_weight * inv_rel;
	nrel_wt = occp->nrel_weight * inv_nrel;
	occp->weight = alpha * occp->orig_weight + rel_wt - nrel_wt;
	if (occp->weight < 0.0)
	    occp->weight = 0.0;
	occp++;
    }

    /* keep all query terms and num_expand terms for each ctype */
    start_ctype = occp = fdbk_info->occ;
    end_occ = fdbk_info->occ + fdbk_info->num_occ;
    for (i = 0; i < num_ctypes; i++) {
	(void) sprintf (param_prefix, "local.feedback.ctype.%ld.", i);
	prefix = param_prefix;
	if (UNDEF == lookup_spec_prefix(saved_spec,
					&prefix_spec_args[0],
					num_prefix_spec_args))
	    return (UNDEF);
	while (occp < end_occ && occp->ctype == i) 
	    occp++;

	/* sort terms by query followed by proximity weight for ctype 0,
	 * rochhio weight for ctype 1 */
	qsort((char *) start_ctype, occp - start_ctype, sizeof(OCC), 
	      compare_q_wt);
	/* now assign rochhio weights to selected ctype 0 terms */
	if (i == 0) {
	    for (j = 0; j < occp - start_ctype && j < num_expand; j++) {
		rel_wt = start_ctype[j].rel_weight * inv_rel;
		nrel_wt = start_ctype[j].nrel_weight * inv_nrel;
		start_ctype[j].weight = alpha * start_ctype[j].orig_weight + 
		    rel_wt - nrel_wt;
		if (start_ctype[j].weight < 0.0)
		    start_ctype[j].weight = 0.0;
	    }
	}
	/* keep (query terms + best num_expand terms; zero out rest */
	for (j = 0; j < occp - start_ctype && start_ctype[j].query; j++);
	/* if a term selected by proximity weight is eliminated by 
	 * rochhio, fill in with some other term */
	for (num_so_far = 0; j<occp-start_ctype && num_so_far<num_expand; j++)
	    if (start_ctype[j].weight > 0.0) num_so_far++;
	for (; j < occp - start_ctype; j++) 
	    start_ctype[j].weight = 0.0;
	/* sort back by con */
	qsort((char *) start_ctype, occp - start_ctype, sizeof(OCC), 
	      compare_con);
	start_ctype = occp;
    }

    /* scale final wts to ensure compatibility with original query wts */
    sum_old = 0;
    for (i = 0; i < fdbk_info->orig_query->num_conwt; i++)
	sum_old += fdbk_info->orig_query->con_wtp[i].wt;
    assert(sum_old > 0);
    sum_new = 0;
    for (i = 0; i < fdbk_info->num_occ; i++) 
	sum_new += fdbk_info->occ[i].weight;
    for (i = 0; i < fdbk_info->num_occ; i++) 
	fdbk_info->occ[i].weight *= sum_old/sum_new;

    PRINT_TRACE (4, print_fdbk_info, fdbk_info);
    PRINT_TRACE (2, print_string, "Trace: leaving wt_fdbk_sel_qprox");

    return (1);
}

int
close_wt_fdbk_sel_qprox (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering/leaving close_wt_fdbk_sel_qprox");
    return (0);
}

static int
compare_con (ptr1, ptr2)
OCC *ptr1;
OCC *ptr2;
{
    if (ptr1->con < ptr2->con)
        return (-1);
    if (ptr1->con > ptr2->con)
        return (1);
    return (0);
}

/* Terms sorted by final weight */
static int
compare_q_wt (ptr1, ptr2)
OCC *ptr1;
OCC *ptr2;
{
    if (ptr1->query) {
        if (ptr2->query)
            return (0);
        return (-1);
    }
    if (ptr2->query)
        return (1);

    if (ptr1->weight > ptr2->weight)
        return (-1);
    if (ptr1->weight < ptr2->weight)
        return (1);
    return (0);
}
