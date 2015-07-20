#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/vecwt_cosine.c,v 11.2 1993/02/03 16:49:34 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Cosine normalize an entire vector
 *1 convert.vecwt_norm.u
 *2 vecwt_cosine (invec, unused, inst)
 *3   VEC *invec;
 *3   char *unused;
 *3   int inst;
 *4 init_vecwt_cosine (spec, unused)
 *5   "convert.vecwt_cosine.major_ctype"
 *5   "convert.weight.trace"
 *4 close_vecwt_cosine (inst)
 *7 
 *7 Normalize the entire vector by the
 *7     (1-slope) + slope*#_cosine/pivot
 *7 where #_cosine is the length (number of terms) of the subvector for
 *7 ctype major_ctype;
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

static SPEC_PARAM spec_args[] = {
    {"convert.vecwt_cosine.major_ctype", getspec_long, (char *) &major_ctype},
    TRACE_PARAM ("convert.weight.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    long major_ctype;             /* Normalize vector at end by this ctype */
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_vecwt_cosine (spec, prefix)
SPEC *spec;
char *prefix;
{
    int new_inst;
    STATIC_INFO *ip;

    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args)) {
        return (UNDEF);
    }
    PRINT_TRACE (2, print_string, "Trace: entering init_vecwt_cosine");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];

    ip->major_ctype = major_ctype;
    if (ip->major_ctype < -1) {
        set_error (SM_ILLPA_ERR, "Illegal major_ctype value", "vecwt_unique");
        return (UNDEF);
    }
    ip->valid_info = 1;
    PRINT_TRACE (2, print_string, "Trace: leaving init_vecwt_cosine");

    return (new_inst);
}

int
vecwt_cosine (invec, unused, inst)
VEC *invec;
char *unused;
int inst;
{
    long i,j;
    long start_ctype;
    STATIC_INFO *ip;
    float sum;

    PRINT_TRACE (2, print_string, "Trace: entering vecwt_cosine");
    PRINT_TRACE (6, print_vector, invec);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "vecwt_cosine");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (invec->num_conwt == 0 || invec->ctype_len == 0) {
        PRINT_TRACE (4, print_vector, invec);
        PRINT_TRACE (2, print_string, "Trace: leaving weight_unique");
        return (0);
    }

    /* Normalize the entire vector */
    sum = 0;
    if (-1 == ip->major_ctype) {
        /* Cosine normalization by the entire vector */
        for (i = 0; i < invec->num_conwt; i++) {
            sum += invec->con_wtp[i].wt * invec->con_wtp[i].wt;
        }
    }
    else if (ip->major_ctype < invec->num_ctype) {
        /* Cosine normalization by only major_ctype */
        start_ctype = 0;
        for (j = 0; j < ip->major_ctype; j++) {
            start_ctype += invec->ctype_len[j];
        }
        for (i = start_ctype;
             i < start_ctype + invec->ctype_len[ip->major_ctype];
             i++) {
            sum += invec->con_wtp[i].wt * invec->con_wtp[i].wt;
        }
    }
    else {
        set_error (SM_INCON_ERR, "Major_ctype is too large", "weight_cos");
        return (UNDEF);
    }
    sum = sqrt (sum);
 
    if (sum == 0.0) {
        /* Return an empty vector */
        invec->num_ctype = 0;
        invec->num_conwt = 0;
    }
    else {
        for (i = 0; i < invec->num_conwt; i++) {
            invec->con_wtp[i].wt /= sum;
        }
    }

    PRINT_TRACE (4, print_vector, invec);
    PRINT_TRACE (2, print_string, "Trace: leaving vecwt_cosine");
    return (1);
}


int
close_vecwt_cosine (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_vecwt_cosine");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_vecwt_cosine");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Close weighting procs and free buffers if this was last close of inst */
    ip->valid_info--;

    PRINT_TRACE (2, print_string, "Trace: leaving close_vecwt_cosine");

    return (0);
}
