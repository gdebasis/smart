#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/token_sect_no.c,v 11.2 1993/02/03 16:51:18 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/
/********************   PROCEDURE DESCRIPTION   ************************
 *0 Construct tokens from a single preparsed document section
 *1 index.token_sect.token_sect_no
 *2 token_sect_no (pp_buf, t_sect, inst)
 *3   SM_BUF *pp_buf;
 *3   SM_TOKENSECT *t_sect;
 *3   int inst;
 *4 init_token_sect_no (spec, unused)
 *5   "index.token_sect.trace"
 *4 close_token_sect_no (inst)

***********************************************************************/


#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "docindex.h"
#include "trace.h"
#include "inst.h"
#include "buf.h"


static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("index.token_sect.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


int
init_token_sect_no (spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args)) {
        return (UNDEF);
    }
    PRINT_TRACE (2, print_string, "Trace: entering/leaving init_token_sect_no");

    return (0);
}


int
token_sect_no (pp_buf, t_sect, inst)
SM_BUF *pp_buf;                    /* Input document section*/
SM_TOKENSECT *t_sect;              /* Output tokens */
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering token_sect_no");
    PRINT_TRACE (6, print_buf, pp_buf);

    t_sect->num_tokens = 0;
    t_sect->tokens = (SM_TOKENTYPE *) NULL;
    t_sect->section_num = UNDEF;

    PRINT_TRACE (4, print_tokensect, t_sect);
    PRINT_TRACE (2, print_string, "Trace: leaving token_sect_no");

    return (0);
}

int
close_token_sect_no (inst)
int inst;
{

    PRINT_TRACE (2, print_string, "Trace: entering close_token_sect_no");

    PRINT_TRACE (2, print_string, "Trace: leaving close_token_sect_no");

    return (0);
}

