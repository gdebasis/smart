#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/weight_inquery.c,v 11.2 1993/02/03 16:49:20 smart Exp $";
#endif

/* Copyright (c) 1994, 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Reweight vector by Inquery style weighting
 *1 convert.wt_tf.o
 *2 tfwt_inquery (vec, unused, inst)
 *3   VEC *vec;
 *3   char *unused;
 *3   int inst;
 *4 INIT_EMPTY
 *4 CLOSE_EMPTY

 *7 For each term in vector, convert its weight based on Inquery's
 *7 weighting function: 0.4*H + 0.6*log(0.5+tf)/log(1.0+max_tf)
 *7 Where:
 *7        H = 1.0 if max_tf < 200
 *7        H = 200/max_tf if max_tf < 200
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "vector.h"

static long kick_in_H;
static SPEC_PARAM spec_args[] = {
    {"weight.kick_in_H", getspec_long, (char *) &kick_in_H},
    TRACE_PARAM ("weight.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

/* For time being, assume no data private to instantiation */
static int num_instantiated = 0;

int
init_tfwt_inquery(spec, unused)
SPEC *spec;
char *unused;
{
    if (num_instantiated) {
        return (num_instantiated++);
    }

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args)) {
        return (UNDEF);
    }

   PRINT_TRACE (2, print_string, "Trace: entering init_tfwt_inquery");
   PRINT_TRACE (2, print_string, "Trace: leaving init_tfwt_inquery");

   return (num_instantiated++);
}

int
tfwt_inquery (vec, unused, inst)
VEC *vec;
char *unused;
int inst;
{
    long i;
    CON_WT *conwt = vec->con_wtp;

    CON_WT *ptr = conwt;
    float max = 0.0;

    for (i = 0; i < vec->num_conwt; i++) {
        if (ptr->wt > max)
            max = ptr->wt;
        ptr++;
    }

    ptr = conwt;
    for (i = 0; i < vec->num_conwt; i++) {
        ptr->wt  = 0.4*(max > kick_in_H ? (float)kick_in_H/max : 1.0) +
	    0.6*log((double)ptr->wt+0.5)/log((double)max+1.0);
        ptr++;
    }
    return (1);
}

int
close_tfwt_inquery(inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_tfwt_inquery");
    num_instantiated --;
    PRINT_TRACE (2, print_string, "Trace: leaving close_tfwt_inquery");
    return (num_instantiated);
}
