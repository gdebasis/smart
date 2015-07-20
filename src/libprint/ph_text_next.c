#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/ph_text_pp_next.c,v 11.2 1993/02/03 16:54:00 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/* print the next text after preprocessing */

#include <fcntl.h>
#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "docindex.h"
#include "trace.h"
#include "io.h"
#include "buf.h"
#include "inst.h"

static char *prefix;
static PROC_INST proc;
static long headings;

static SPEC_PARAM_PREFIX prefix_spec_args[] = {
    { &prefix, "next_preparse",	getspec_func,	(char *) &proc.ptab},
    { &prefix, "headings",	getspec_bool,	(char *) &headings},
    };
static int num_prefix_spec_args = sizeof (prefix_spec_args) /
         sizeof (prefix_spec_args[0]);

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("print.indiv.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    PROC_INST proc;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_print_text_pp_next (spec, qd_prefix)
SPEC *spec;
char *qd_prefix;
{
    char prefix_string[PATH_LEN];
    STATIC_INFO *ip;
    int new_inst;

    NEW_INST(new_inst);
    if (UNDEF == new_inst)
	return(UNDEF);
    
    ip = &info[new_inst];

    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec(spec, &spec_args[0], num_spec_args)) {
	return(UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_print_text_pp_next");

    prefix = prefix_string;
    (void) sprintf(prefix_string, "print.%s", qd_prefix);
    if (UNDEF == lookup_spec_prefix(spec, &prefix_spec_args[0],
					num_prefix_spec_args))
	return(UNDEF);

    /* Call all initialization procedures */
    ip->proc.ptab = proc.ptab;
    ip->proc.inst = proc.ptab->init_proc(spec, qd_prefix);
    if (UNDEF == ip->proc.inst) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving init_print_text_pp_next");

    ip->valid_info = 1;
    return(new_inst);
}

int
print_text_pp_next (unused, output, inst)
char *unused;
SM_BUF *output;
int inst;
{
    STATIC_INFO *ip;
    SM_INDEX_TEXTDOC pp_doc;
    int status;

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "print_text_pp_next");
	return(UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering print_text_pp_next");

    /* 0 is better than random numbers... */
    pp_doc.id_num.id = 0;
    EXT_NONE(pp_doc.id_num.ext);

    status = ip->proc.ptab->proc (unused, &pp_doc, ip->proc.inst);
    if (1 != status) {
	PRINT_TRACE (2, print_string, "Trace: leaving print_text_pp_next");
        return (status);
    }
    if (headings) print_int_textdoc (&pp_doc, output);
    else print_int_textdoc_nohead (&pp_doc, output);

    PRINT_TRACE (2, print_string, "Trace: leaving print_text_pp_next");
    return (1);
}


int
close_print_text_pp_next (inst)
int inst;
{
    STATIC_INFO *ip;

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "close_print_text_pp_next");
	return(UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering close_print_text_pp_next");

    if (UNDEF == ip->proc.ptab->close_proc (ip->proc.inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_print_text_pp_next");
    return (0);
}

