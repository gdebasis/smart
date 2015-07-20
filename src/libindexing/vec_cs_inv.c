#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/vec_collstat.c,v 11.2 1993/02/03 16:51:14 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Store vec in inverted file and add collection stat info
 *1 convert.tup.vec_collstat_inv
 *2 vec_collstat_inv (vector, unused, inst)
 *3   VEC *vector;
 *3   char *unused;
 *3   int inst;
 *4 init_vec_collstat_inv (spec, param_prefix)
 *4 close_vec_collstat_inv(inst)
 *7 Add info from vector to collection file by calling vec_collstat().
 *7 Add vector to inverted file by calling vec_inv().
 *7 Normally, param_prefix will have some value like "ctype.1." 
 *7 UNDEF returned if error, else 0.
**********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "vector.h"
#include "inst.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("store.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    int collstat_inst;
    int inv_inst;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

extern int init_vec_collstat(), vec_collstat(), close_vec_collstat();
extern int init_vec_inv(), vec_inv(), close_vec_inv();

int
init_vec_collstat_inv (spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    STATIC_INFO *ip;
    int new_inst;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);
    
    PRINT_TRACE (2, print_string, "Trace: entering init_vec_collstat_inv");
    
    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];

    if (UNDEF == (ip->collstat_inst = init_vec_collstat (spec, param_prefix)))
        return (UNDEF);
    if (UNDEF == (ip->inv_inst = init_vec_inv (spec, param_prefix)))
        return (UNDEF);
    
    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_vec_collstat_inv");

    return (new_inst);
}

int
vec_collstat_inv (vector, unused, inst)
VEC *vector;
char *unused;
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering vec_collstat_inv");
    PRINT_TRACE (6, print_vector, vector);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "vec_collstat_inv");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (UNDEF == vec_collstat (vector, unused, ip->collstat_inst))
        return (UNDEF);
    if (UNDEF == vec_inv (vector, unused, ip->inv_inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving vec_collstat_inv");

    return (1);
}

int
close_vec_collstat_inv(inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_vec_collstat_inv");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_vec_collstat_inv");
        return (UNDEF);
    }

    ip  = &info[inst];
    ip->valid_info--;

    if (UNDEF == close_vec_collstat (ip->collstat_inst) ||
        UNDEF == close_vec_inv (ip->inv_inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_vec_collstat_inv");
    return (0);
}
