#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libfeedback/wt_fdbk.c,v 11.2 1993/02/03 16:50:03 smart Exp $";
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
 *1 local.feedback.weight.fdbk_sel_cWITHrel
 *2 wt_fdbk_sel_cWITHrel (unused1, fdbk_info, inst)
 *3   char *unused1;
 *3   FEEDBACK_INFO *fdbk_info;
 *3   int inst;
 *4 init_wt_fdbk_sel_cWITHrel (spec, unused)
 *5   "feedback.alpha"
 *5   "feedback.beta"
 *5   "feedback.gamma"
 *5   "feedback.weight.trace"
 *4 close_wt_fdbk_sel_cWITHrel (inst)
 *7 Same as feedback.weight.fdbk (libfeedback/wt_fdbk.c), except that
 *7 selection of ctype dependent num_expand terms occurs here. 
 *7 Term selection based on final Rochhio weight.
***********************************************************************/

static float alpha;
static float beta;
static float gammavar;

static SPEC_PARAM spec_args[] = {
    {"feedback.alpha",         getspec_float, (char *) &alpha},
    {"feedback.beta",          getspec_float, (char *) &beta},
    {"feedback.gamma",         getspec_float, (char *) &gammavar},
    TRACE_PARAM ("feedback.weight.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

int
init_wt_fdbk_sel_cWITHrel (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_wt_fdbk_sel_cWITHrel");

    PRINT_TRACE (2, print_string, "Trace: leaving init_wt_fdbk_sel_cWITHrel");
    return (1);
}

int
wt_fdbk_sel_cWITHrel (unused, fdbk_info, inst)
char *unused;
FEEDBACK_INFO *fdbk_info;
int inst;
{
    long i;
    double rel_wt, nrel_wt, sum_old, sum_new, cWITHrel;

    PRINT_TRACE (2, print_string, "Trace: entering wt_fdbk_sel_cWITHrel");

    for (i = 0; i < fdbk_info->num_occ; i++) {
        if (fdbk_info->num_rel > 0) 
            rel_wt = beta * fdbk_info->occ[i].rel_weight /
                fdbk_info->num_rel;
        else
            rel_wt = 0.0;

        if (fdbk_info->tr->num_tr > fdbk_info->num_rel) 
            nrel_wt = gammavar * fdbk_info->occ[i].nrel_weight /
                (fdbk_info->tr->num_tr - fdbk_info->num_rel);
        else
            nrel_wt = 0.0;

        fdbk_info->occ[i].weight = alpha * fdbk_info->occ[i].orig_weight + 
                                   rel_wt - nrel_wt;

#define CORREL(p12,p1,p2) (p1==1.0 || p2==1.0 || p1==0.0 || p2==0.0 ? 0.0: \
                           (p12 - p1 * p2) / \
                           sqrt ((double)(p1*(1.0 - p1) * p2*(1.0 - p2))))

	cWITHrel = CORREL (((double) fdbk_info->occ[i].rel_ret / fdbk_info->tr->num_tr),
((double) (fdbk_info->occ[i].rel_ret+fdbk_info->occ[i].nrel_ret) / fdbk_info->tr->num_tr),
               ((double) fdbk_info->num_rel / fdbk_info->tr->num_tr));

        fdbk_info->occ[i].weight *= (float) cWITHrel;

        if (fdbk_info->occ[i].weight < 0.0)
            fdbk_info->occ[i].weight = 0.0;
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
    PRINT_TRACE (2, print_string, "Trace: leaving wt_fdbk_sel_cWITHrel");

    return (1);
}

int
close_wt_fdbk_sel_cWITHrel (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering/leaving close_wt_fdbk_sel_cWITHrel");
    return (0);
}
