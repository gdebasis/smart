#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/next_vecid_1.c,v 11.2 1993/02/03 16:50:49 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Give next id for a new vector, starting at 1
 *1 index.next_vecid.next_vecid_1
 *2 next_vecid_1 (unused, id, inst)
 *3   char *unused;
 *3   long *id;
 *3   int inst;
 *4 init_next_vecid_1 (spec, unused)
 *5   "index.next_vecid.trace"
 *4 close_next_vecid (inst)
 *8 First return 1, and then increment by 1 with each call
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "inst.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("index.next_vecid.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static long vecid_start;
static SPEC_PARAM start_spec_args[] = {
    {"vecid_start",           getspec_long,    (char *) &vecid_start},
    };
static int num_start_spec_args = sizeof (start_spec_args) / 
    sizeof (start_spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    long next_vecid;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_next_vecid_1 (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;

    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_next_vecid_1");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];

    /* See if a non-1 value was specified for vecid_start;  If so, then
       use that.  Not an error if no value was found */
    if (UNDEF == lookup_spec (spec,
                              &start_spec_args[0],
                              num_start_spec_args)) {
        clr_err();
        ip->next_vecid = 1;
    }
    else
        ip->next_vecid = vecid_start;

    ip->valid_info++;

    PRINT_TRACE (4, print_long, &ip->next_vecid);
    PRINT_TRACE (2, print_string, "Trace: leaving init_next_vecid_1");
    return (new_inst);
}

int
next_vecid_1 (unused, id, inst)
char *unused;
long *id;
int inst;
{
    STATIC_INFO *ip;
    PRINT_TRACE (2, print_string, "Trace: entering next_vecid_1");
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "next_vecid_1");
        return (UNDEF);
    }
    ip  = &info[inst];

    *id = ip->next_vecid++;

    PRINT_TRACE (4, print_long, id);
    PRINT_TRACE (2, print_string, "Trace: leaving next_vecid_1");

    return (0);
}

int
close_next_vecid_1 (inst)
int inst;
{
    STATIC_INFO *ip;
    PRINT_TRACE (2, print_string, "Trace: entering close_next_vecid_1");
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_next_vecid_1");
        return (UNDEF);
    }

    ip  = &info[inst];
    ip->valid_info--;

    ip->next_vecid = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving close_next_vecid_1");

    return (0);
}



