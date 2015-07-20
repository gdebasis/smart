#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfeedback/wt_fdbk.c,v 11.2 1993/02/03 16:50:03 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "spec.h"
#include "trace.h"
#include "feedback.h"

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Low-level feedback weighting procedure - Roccio
 *1 feedback.weight.fdbk
 *1 feedback.weight.roccio
 *2 wt_fdbk (unused1, fdbk_info, inst)
 *3   char *unused1;
 *3   FEEDBACK_INFO *fdbk_info;
 *3   int inst;
 *4 init_wt_fdbk (spec, unused)
 *5   "feedback.alpha"
 *5   "feedback.beta"
 *5   "feedback.gamma"
 *5   "feedback.weight.trace"
 *4 close_wt_fdbk (inst)
 *7 compute the feedback weight for an individual query term i according
 *7 to the formula (Roccio feedback):
 *7                     sum(term weight in rel)        sum(term weight in nrel)
 *7 Q' = A * Qold + B * ------------------------ - C * ------------------------
 *7                        num_rel_doc                     num_nrel_doc
 *7
 *7  where num_rel_doc is the total number of relevant docs retrieved
 *7  where num_nrel_doc is the total number of non-relevant docs retrieved
 *7  and A,B,C are constants (feedback.alpha, feedback.beta, feedback.gamma)
 *7
 *7 fdbk_info gives all occurrence information for each term, as well as
 *7 storing the resulting weight.
 *9 Warning: occurrence info must be for nonrel-retrieved docs only
 *9 (cannot use inv_occ_all)
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
init_wt_fdbk (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_wt_fdbk");
    PRINT_TRACE (6, print_spec, spec);

    PRINT_TRACE (2, print_string, "Trace: leaving init_wt_fdbk");
    return (1);
}

int
wt_fdbk (unused, fdbk_info, inst)
char *unused;
FEEDBACK_INFO *fdbk_info;
int inst;
{
    long i;
    double rel_wt, nrel_wt;

    PRINT_TRACE (2, print_string, "Trace: entering wt_fdbk");

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
        if (fdbk_info->occ[i].weight < 0.0)
            fdbk_info->occ[i].weight = 0.0;
    }

    PRINT_TRACE (4, print_fdbk_info, fdbk_info);
    PRINT_TRACE (2, print_string, "Trace: leaving wt_fdbk");

    return (1);
}

int
close_wt_fdbk (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering/leaving close_wt_fdbk");
    return (0);
}

