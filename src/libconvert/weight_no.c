#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/weight_no.c,v 11.2 1993/02/03 16:49:35 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 No-op weighting procedure.  Copy entire vector without re-weighting.
 *1 convert.weight.weight_no
 *2 weight_no (invec, outvec, inst)
 *3   VEC *invec;
 *3   VEC *outvec;
 *3   int inst;
 *4 init_weight_no (spec, unused)
 *5   "convert.weight.trace"
 *4 close_weight_no (inst)
 *7 Copy the entire vector from invec to outvec.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "spec.h"
#include "trace.h"
#include "vector.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("convert.weight.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


int
init_weight_no (spec, unused)
SPEC *spec;
char *unused;
{
    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args)) {
        return (UNDEF);
    }
    PRINT_TRACE (2, print_string, "Trace: entering/leaving init_weight_no");

    return (0);
}

int
weight_no (invec, outvec, inst)
VEC *invec;
VEC *outvec;
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering weight_no");
    PRINT_TRACE (6, print_vector, invec);

    if (outvec != NULL) {
        outvec->id_num = invec->id_num;
        outvec->num_ctype = invec->num_ctype;
        outvec->num_conwt = invec->num_conwt;
        outvec->con_wtp   = invec->con_wtp;
        outvec->ctype_len = invec->ctype_len;
        PRINT_TRACE (4, print_vector, outvec);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving weight_no");
    return (1);
}


int
close_weight_no (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering/leaving close_weight_no");
    return (0);
}



