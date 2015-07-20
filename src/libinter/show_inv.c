#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libinter/show_inv.c,v 11.0 1992/07/21 18:21:50 chrisb Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "io.h"
#include "spec.h"
#include "inv.h"
#include "inter.h"
#include "inst.h"
#include "trace.h"
#include "smart_error.h"

static long num_ctypes;

static SPEC_PARAM spec_args[] = {
    {"num_ctypes",          getspec_long,    (char *) &num_ctypes},
    TRACE_PARAM ("inter.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    long num_ctypes;
    int *ctype_inst;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_show_inv (spec, unused)
SPEC *spec;
char *unused;
{
    long i;
    char param_prefix[PATH_LEN];
    STATIC_INFO *ip;
    int new_inst;

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_show_inv");

    ip->num_ctypes = num_ctypes;
    if (NULL == (ip->ctype_inst = (int *)
                 malloc ((unsigned) ip->num_ctypes * sizeof (int))))
        return (UNDEF);

    for (i = 0; i < ip->num_ctypes; i++) {
        (void) sprintf (param_prefix, "inter.ctype.%ld.", i);
        ip->ctype_inst[i] = init_con_inv (spec, param_prefix);
    }

    ip->valid_info++;

    PRINT_TRACE (2, print_string, "Trace: leaving init_show_inv");
    return (new_inst);
}


int
close_show_inv (inst)
int inst;
{
    long i;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_show_inv");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.show_inv");
        return (UNDEF);
    }
    ip  = &info[inst];

    for (i = 0; i < ip->num_ctypes; i++) {
        if (UNDEF != ip->ctype_inst[i] &&
            UNDEF == close_con_inv (ip->ctype_inst[i]))
            return (UNDEF);
    }

    (void) free ((char *) ip->ctype_inst);

    PRINT_TRACE (2, print_string, "Trace: leaving close_show_inv");
    return (1);
}

int
show_inv (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    long con, ctype;
    INV inv;
    char temp_buf[PATH_LEN];
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering show_inv");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.show_inv");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (is->num_command_line < 2) {
        if (UNDEF == add_buf_string ("No con specified\n", &is->err_buf))
            return (UNDEF);
        return (0);
    }
    con = atol (is->command_line[1]);

    if (is->num_command_line == 2)
        ctype = 0;
    else
        ctype = atol (is->command_line[2]);
    
    if (ctype < 0 || ctype >= num_ctypes) {
        if (UNDEF == add_buf_string ("Illegal ctype specified\n",&is->err_buf))
            return (UNDEF);
        return (0);
    }

    if (UNDEF == con_inv (&con, &inv, ip->ctype_inst[ctype])) {
        (void) sprintf (temp_buf, "Can't find con %ld\n", con);
        if (UNDEF == add_buf_string (temp_buf, &is->err_buf))
            return (UNDEF);
        return (0);
    }

    print_inv (&inv, &is->output_buf);

    PRINT_TRACE (2, print_string, "Trace: leaving show_inv");
    return (1);
}

