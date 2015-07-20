#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/ph_vec_dict.c,v 11.2 1993/02/03 16:54:04 smart Exp $";
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
#include "proc.h"
#include "spec.h"
#include "docindex.h"
#include "trace.h"
#include "context.h"
#include "io.h"
#include "buf.h"
#include "vector.h"
#include "docdesc.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("print.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static SM_INDEX_DOCDESC doc_desc;
static int *contok_inst;

static int num_inst = 0;

int
init_print_vec_dict (spec, unused)
SPEC *spec;
char *unused;
{
    char param_prefix[PATH_LEN];
    long i;

    if (num_inst++) {
        PRINT_TRACE (2, print_string,
                     "Trace: entering/leaving init_print_vec_dict");
        return (0);
    }

    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args))
        return (UNDEF);
    
    PRINT_TRACE (2, print_string, "Trace: entering init_print_vec_dict");

    if (UNDEF == lookup_spec_docdesc (spec, &doc_desc))
        return (UNDEF);

    /* Reserve space for the instantiation ids of the called procedures. */
    if (NULL == (contok_inst = (int *)
                 malloc ((unsigned) doc_desc.num_ctypes * sizeof (int))))
        return (UNDEF);

     for (i = 0; i < doc_desc.num_ctypes; i++) {
        /* Set param_prefix to be current parameter path for this ctype.
           This will then be used by the con_to_token routine to lookup
           parameters it needs. */
        (void) sprintf (param_prefix, "index.ctype.%ld.", i);
        if (UNDEF == (contok_inst[i] =
                      doc_desc.ctypes[i].con_to_token->init_proc
                      (spec, param_prefix)))
            return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving init_print_vec_dict");
    return (0);
}

int
print_vec_dict (vec, output, inst)
VEC *vec;
SM_BUF *output;
int inst;
{
    long i;
    CON_WT *conwtp = vec->con_wtp;
    long ctype;
    char buf[PATH_LEN];
    char *token;
	CTYPE_INFO* ctype_info;

    PRINT_TRACE (2, print_string, "Trace: entering print_vec_dict");

    for (ctype = 0; ctype < vec->num_ctype; ctype++) {
        for (i = 0; i < vec->ctype_len[ctype]; i++) {
			ctype_info = ctype < doc_desc.num_ctypes? &doc_desc.ctypes[ctype] : NULL;
            if (ctype_info) {
				if (UNDEF == ctype_info->con_to_token->proc (&conwtp->con, &token, contok_inst[ctype])) {
					token = "Not in dictionary";
					clr_err();
            	}
			}
			else {
                token = "pos info";
                clr_err();
			}
            (void) sprintf (buf, "%ld\t%ld\t%ld\t%8.5f\t%s\n",
                    vec->id_num.id, ctype,
                    conwtp->con, conwtp->wt, token);
            if (output == NULL)
                fputs (buf, stdout);
            else {
                if (UNDEF == add_buf_string (buf, output))
                    return (UNDEF);
            }
            if (++conwtp > &vec->con_wtp[vec->num_conwt]) {
                (void) fprintf (stderr,
                 "within print_vec_dict: %ld: Inconsistant vector length\n",
                         vec->id_num.id);
            }
        }

    }
    if (conwtp != &vec->con_wtp[vec->num_conwt]) {
        (void) fprintf (stderr,
                 "print_vec_dict: %ld: Inconsistant final vector length\n",
                 vec->id_num.id);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving print_vec_dict");
    return (1);
}


int
close_print_vec_dict (inst)
int inst;
{
    long i;

    PRINT_TRACE (2, print_string, "Trace: entering close_print_vec_dict");

    if (--num_inst == 0) {
        for (i = 0; i < doc_desc.num_ctypes; i++) {
            if (UNDEF == doc_desc.ctypes[i].con_to_token->
                close_proc(contok_inst[i]))
                return (UNDEF);
        }
	(void) free ((char *) contok_inst);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_print_vec_dict");
    return (0);
}


