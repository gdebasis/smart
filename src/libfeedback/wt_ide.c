#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfeedback/wt_ide.c,v 11.2 1993/02/03 16:49:58 smart Exp $";
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

/*
 * compute the feedback weight for an individual query term i according
 * to the formula (Ide feedback)
 *                    
 * Q' = A * Qold + B * sum(term weight in rel) - C * (term weight in top nrel)
 *
 *  where A,B,C are constants (feedback.alpha, feedback.beta, feedback.gamma)
 */

static float alpha;
static float beta;
static float gammavar;

static SPEC_PARAM spec_args[] = {
    {"feedback.alpha",         getspec_float, (char *) &alpha},
    {"feedback.beta",          getspec_float, (char *) &beta},
    {"feedback.gamma",          getspec_float, (char *) &gammavar},
    TRACE_PARAM ("feedback.weight.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);


int
init_wt_ide (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_wt_ide");

    PRINT_TRACE (2, print_string, "Trace: leaving init_wt_ide");
    return (1);
}

int
wt_ide (unused, fdbk_info, inst)
char *unused;
FEEDBACK_INFO *fdbk_info;
int inst;
{
    long i;

    PRINT_TRACE (2, print_string, "Trace: entering wt_ide");

    for (i = 0; i < fdbk_info->num_occ; i++) {
        fdbk_info->occ[i].weight = alpha * fdbk_info->occ[i].orig_weight + 
                                   beta * fdbk_info->occ[i].rel_weight -
                                   gammavar * fdbk_info->occ[i].nrel_weight;
        if (fdbk_info->occ[i].weight < 0.0)
            fdbk_info->occ[i].weight = 0.0;
    }

    PRINT_TRACE (4, print_fdbk_info, fdbk_info);
    PRINT_TRACE (2, print_string, "Trace: leaving wt_ide");

    return (1);
}

int
close_wt_ide (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering/leaving close_wt_ide");
    return (0);
}
