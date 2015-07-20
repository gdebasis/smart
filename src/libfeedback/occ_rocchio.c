#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfeedback/occ_rocchio.c,v 11.2 1993/02/03 16:50:01 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "inv.h"
#include "tr_vec.h"
#include "vector.h"
#include "feedback.h"
#include "spec.h"
#include "trace.h"

/*
    Same as occ_inv, except doesn't use inverted file to get info.
    Instead, this indexes all the retrieved non-relevant docs at
    run time.
*/

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("feedback.occ_info.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static int did_vec_inst;

int
init_occ_rocchio (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_occ_rocchio");
    if (UNDEF == (did_vec_inst = init_did_vec (spec, (char *) NULL)))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_occ_rocchio");
    return (1);
}

int
occ_rocchio (unused, fdbk_info, inst)
char *unused;
FEEDBACK_INFO *fdbk_info;
int inst;
{
    long i, j, ctype;
    VEC dvec;
    CON_WT *conwt;
    long top_iter;
    OCC *occ_ptr, *end_occ_ptr;
    
    PRINT_TRACE (2, print_string, "Trace: entering occ_rocchio");

    top_iter = -1;
    for (i = 0; i < fdbk_info->tr->num_tr; i++)
        if (fdbk_info->tr->tr[i].iter >= top_iter)
            top_iter = fdbk_info->tr->tr[i].iter;

    /* Find a retrieved nonrel doc in last iteration and add it */
    for (i = 0; i < fdbk_info->tr->num_tr; i++) {
        if (fdbk_info->tr->tr[i].rel == 0 &&
            fdbk_info->tr->tr[i].iter == top_iter) {
	    /* Get vector for the nonrel doc */
	    if (1 != did_vec (&fdbk_info->tr->tr[i].did, &dvec, did_vec_inst))
		return (UNDEF);

	    /* Go through dvec and fdbk_info->occ in parallel (both sorted by 
	       ctype,con) and add info for those in both to fdbk_info->occ */
	    occ_ptr = fdbk_info->occ;
	    end_occ_ptr = &fdbk_info->occ[fdbk_info->num_occ];
	    conwt = dvec.con_wtp;
	    for (ctype = 0; ctype < dvec.num_ctype; ctype++) {
		for (j = 0; j < dvec.ctype_len[ctype]; j++) {
		    while (occ_ptr < end_occ_ptr && occ_ptr->ctype < ctype)
			occ_ptr++;
		    while (occ_ptr < end_occ_ptr &&
			   occ_ptr->ctype == ctype &&
			   occ_ptr->con < conwt->con)
			occ_ptr++;
		    if (occ_ptr < end_occ_ptr &&
			occ_ptr->ctype == ctype &&
			occ_ptr->con == conwt->con) {
			occ_ptr->nrel_weight += conwt->wt;
			occ_ptr->nrel_ret++;
		    }
		    if (occ_ptr >= end_occ_ptr)
			break;
		    conwt++;
		}
	    }
	}
    }

    PRINT_TRACE (4, print_fdbk_info, fdbk_info);
    PRINT_TRACE (2, print_string, "Trace: leaving occ_rocchio");

    return (1);
}


int
close_occ_rocchio (inst)
int inst;
{

    PRINT_TRACE (2, print_string, "Trace: entering close_occ_rocchio");

    if (UNDEF == close_did_vec (did_vec_inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_occ_rocchio");
    return (1);
}
