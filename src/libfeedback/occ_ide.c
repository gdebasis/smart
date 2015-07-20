#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfeedback/occ_ide.c,v 11.2 1993/02/03 16:50:01 smart Exp $";
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

/* Get occ info for top nonrel doc only
*/

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("feedback.occ_info.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static int did_vec_inst;

int
init_occ_ide (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_occ_ide");
    if (UNDEF == (did_vec_inst = init_did_vec (spec, (char *) NULL)))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_occ_ide");
    return (1);
}

int
occ_ide (unused, fdbk_info, inst)
char *unused;
FEEDBACK_INFO *fdbk_info;
int inst;
{
    long i, ctype;
    VEC dvec;
    CON_WT *conwt;
    long top_nonrel_did, top_nonrel_rank, top_iter;
    OCC *occ_ptr, *end_occ_ptr;
    
    PRINT_TRACE (2, print_string, "Trace: entering occ_ide");

    /* Find top ranked nonrel did of last iteration */
    top_nonrel_did = -1;
    top_nonrel_rank = fdbk_info->tr->num_tr;
    top_iter = -1;
    for (i = 0; i < fdbk_info->tr->num_tr; i++) {
        if (fdbk_info->tr->tr[i].rel == 0 &&
            fdbk_info->tr->tr[i].iter >= top_iter &&
            fdbk_info->tr->tr[i].rank < top_nonrel_rank) {
            top_nonrel_rank = fdbk_info->tr->tr[i].rank;
            top_nonrel_did = fdbk_info->tr->tr[i].did;
            top_iter = fdbk_info->tr->tr[i].iter;
        }
    }
    if (top_nonrel_did == -1) {
        /* Only rel docs were retrieved! */
        return (0);
    }

    /* Get vector for top nonrel doc */
    if (1 != did_vec (&top_nonrel_did, &dvec, did_vec_inst))
        return (UNDEF);

    /* Go through dvec and fdbk_info->occ in parallel (both sorted by 
       ctype,con) and add info for those in both to fdbk_info->occ */
    occ_ptr = fdbk_info->occ;
    end_occ_ptr = &fdbk_info->occ[fdbk_info->num_occ];
    conwt = dvec.con_wtp;
    for (ctype = 0; ctype < dvec.num_ctype; ctype++) {
        for (i = 0; i < dvec.ctype_len[ctype]; i++) {
            while (occ_ptr < end_occ_ptr && occ_ptr->ctype < ctype)
                occ_ptr++;
            while (occ_ptr < end_occ_ptr &&
                   occ_ptr->ctype == ctype &&
                   occ_ptr->con < conwt->con)
                occ_ptr++;
            if (occ_ptr < end_occ_ptr &&
                occ_ptr->ctype == ctype &&
                occ_ptr->con == conwt->con) {
                occ_ptr->nrel_weight = conwt->wt;
                occ_ptr->nrel_ret = 1;
            }
            if (occ_ptr >= end_occ_ptr)
                break;
            conwt++;
        }
    }

    PRINT_TRACE (4, print_fdbk_info, fdbk_info);
    PRINT_TRACE (2, print_string, "Trace: leaving occ_ide");

    return (1);
}


int
close_occ_ide (inst)
int inst;
{

    PRINT_TRACE (2, print_string, "Trace: entering close_occ_ide");

    if (UNDEF == close_did_vec (did_vec_inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_occ_ide");
    return (1);
}
