#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfeedback/wt_fdbk_lm.c,v 11.2 1993/02/03 16:49:58 smart Exp $";
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
#include "dir_array.h"
#include "collstat.h"
#include "inv.h"
#include "smart_error.h"

float origTermLambda;  	// lambda to be used for the orig query terms
float lambda ;			// lambda to be used for the feedback terms

static SPEC_PARAM spec_args[] = {
    {"retrieve.lm.lambda", getspec_float, (char*)&origTermLambda},
    {"feedback.lm.lambda", getspec_float, (char*)&lambda},
    TRACE_PARAM ("feedback.weight.lm.trace")
};
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

int
init_wt_fdbk_lm (spec, unused)
SPEC *spec;
char *unused;
{
    PRINT_TRACE (2, print_string, "Trace: entering init_wt_fdbk_lm");

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_wt_fdbk_lm");
    return (1);
}

int
wt_fdbk_lm (unused, fdbk_info, inst)
char *unused;
FEEDBACK_INFO *fdbk_info;
int inst;
{
    long    i, k ;
	VEC*    qvec = fdbk_info->orig_query ;

    PRINT_TRACE (2, print_string, "Trace: entering wt_fdbk_lm");

	// Iterate for each query term and the set of relevant documents 
    for (i = 0; i < fdbk_info->num_occ; i++) {
		fdbk_info->occ[i].weight = fdbk_info->occ[i].query ? origTermLambda : lambda;
	}
	
    PRINT_TRACE (4, print_fdbk_info, fdbk_info);
    PRINT_TRACE (2, print_string, "Trace: leaving wt_fdbk_lm");

    return (1);
}

int
close_wt_fdbk_lm (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_wt_fdbk_lm");
    PRINT_TRACE (2, print_string, "Trace: leaving close_wt_fdbk_lm");
    return (0);
}
