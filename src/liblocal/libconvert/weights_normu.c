#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/vecwt_unique.c,v 11.2 1993/02/03 16:49:34 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Unique normalize an entire vector
 *1 local.convert.vecwt_norm.u
 *2 vecwt_unique (invec, unused, inst)
 *3   VEC *invec;
 *3   char *unused;
 *3   int inst;
 *4 init_vecwt_unique (spec, unused)
 *5   "convert.vecwt_unique.major_ctype"
 *5   "convert.vecwt_unique.pivot"
 *5   "convert.vecwt_unique.slope"
 *5   "convert.weight.trace"
 *4 close_vecwt_unique (inst)
 *7 
 *7 Normalize the entire vector by the
 *7     (slope/(1-slope)) * (#_unique/pivot) 
 *7 where #_unique is the length (number of terms) of the subvector for
 *7 ctype major_ctype;
 *7 *** NOTE DIFF. FROM USUAL FORMULA ***
 *7 Using this formula ensures that the idf factor is in [0,1].
 *7 Used in conjunction with tf factor M and normalization factor U.
 *7 Using this combination (MNU) retains the goodness of L(.)u schemes
 *7 while ensuring that all term weights are in [0,1].
 *7 If major_ctype is -1, the terms of all ctypes in the vector are
 *7 used in the normalization.
 *7 This procedure is usually used in circumstances where the vector
 *7 includes optional extra information that should not affect the
 *7 normalization of the major information, but the extra information must be
 *7 made commensurate with the major information.
 *7 If the major_ctype has length 0, an empty vector is returned.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "vector.h"
#include "trace.h"
#include "inst.h"

static long major_ctype;
static float pivot;
static float slope;

static SPEC_PARAM spec_args[] = {
    {"convert.vecwt_unique.major_ctype", getspec_long, (char *) &major_ctype},
    {"convert.vecwt_unique.pivot", getspec_float, (char *) &pivot},
    {"convert.vecwt_unique.slope", getspec_float, (char *) &slope},
    TRACE_PARAM ("convert.weight.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    long major_ctype;             /* Normalize vector at end by this ctype */
    float pivot;
    float slope;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_local_vecwt_unique (spec, prefix)
SPEC *spec;
char *prefix;
{
    int new_inst;
    STATIC_INFO *ip;

    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args)) {
        return (UNDEF);
    }
    PRINT_TRACE (2, print_string, "Trace: entering init_vecwt_unique");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];

    ip->major_ctype = major_ctype;
    ip->pivot = pivot;
    ip->slope = slope;

    if (ip->major_ctype < -1) {
        set_error (SM_ILLPA_ERR, "Illegal major_ctype value", "vecwt_unique");
        return (UNDEF);
    }
    if (ip->pivot <= 0.0) {
        set_error (SM_ILLPA_ERR, "Illegal pivot value", "vecwt_unique");
        return (UNDEF);
    }
    if (ip->slope <= 0.0 || ip->slope > 1.0) {
        set_error (SM_ILLPA_ERR, "Illegal slope value", "vecwt_unique");
        return (UNDEF);
    }

    ip->valid_info = 1;
    PRINT_TRACE (2, print_string, "Trace: leaving init_vecwt_unique");

    return (new_inst);
}

int
local_vecwt_unique (invec, unused, inst)
VEC *invec;
char *unused;
int inst;
{
    long i;
    STATIC_INFO *ip;
    float sum;

    PRINT_TRACE (2, print_string, "Trace: entering vecwt_unique");
    PRINT_TRACE (6, print_vector, invec);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "vecwt_unique");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Normalize the entire vector */
    sum = 0;
    if (-1 == ip->major_ctype)
        sum = invec->num_conwt;
    else if (ip->major_ctype < invec->num_ctype)
	sum = invec->ctype_len[ip->major_ctype];
    else {
        set_error (SM_INCON_ERR, "Major_ctype is too large", "vecwt_unique");
        return (UNDEF);
    }

    if (sum == 0.0) {
        /* Return an empty vector */
        invec->num_ctype = 0;
        invec->num_conwt = 0;
    }
    else {
	sum = 1.0 + (ip->slope/(1.0 - ip->slope)) * (sum/ip->pivot);
        for (i = 0; i < invec->num_conwt; i++)
	    invec->con_wtp[i].wt /= sum;
    }

    PRINT_TRACE (4, print_vector, invec);
    PRINT_TRACE (2, print_string, "Trace: leaving vecwt_unique");
    return (1);
}


int
close_local_vecwt_unique (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_vecwt_unique");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_vecwt_unique");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Close weighting procs and free buffers if this was last close of inst */
    ip->valid_info--;

    PRINT_TRACE (2, print_string, "Trace: leaving close_vecwt_unique");

    return (0);
}
