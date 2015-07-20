#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/expand_no.c,v 11.2 1993/02/03 16:51:10 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 No-op vector expandsion procedure.  Copy entire vector without expandsion.
 *1 index.expand.none
 *2 expand_no (invec, outvec, inst)
 *3   SM_VECTOR *invec;
 *3   SM_VECTOR *outvec;
 *3   int inst;
 *4 init_expand_no (spec, unused)
 *5   "convert.expand.trace"
 *4 close_expand_no (inst)
 *7 Copy the entire vector from invec to outvec.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "spec.h"
#include "trace.h"
#include "vector.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("convert.expand.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


int
init_expand_no (spec, unused)
SPEC *spec;
char *unused;
{
    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args)) {
        return (UNDEF);
    }
    PRINT_TRACE (2, print_string, "Trace: entering/leaving init_expand_no");

    return (0);
}

int
expand_no (invec, outvec, inst)
SM_VECTOR *invec;
SM_VECTOR *outvec;
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering expand_no");
    PRINT_TRACE (6, print_vector, invec);

    outvec->id_num = invec->id_num;
    outvec->num_ctype = invec->num_ctype;
    outvec->num_conwt = invec->num_conwt;
    outvec->con_wtp   = invec->con_wtp;
    outvec->ctype_len = invec->ctype_len;
    
    PRINT_TRACE (4, print_vector, outvec);
    PRINT_TRACE (2, print_string, "Trace: leaving expand_no");
    return (1);
}


int
close_expand_no (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering/leaving close_expand_no");
    return (0);
}



