#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfeedback/local_exp_reldoc.c,v 11.0 1992/07/21 18:20:39 chrisb Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley.

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "tr_vec.h"
#include "vector.h"
#include "feedback.h"
#include "spec.h"
#include "trace.h"

/* Expand query by adding all terms occurring in rel docs */

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("feedback.expand.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);


static int add_doc();

static int did_vec_inst;

int
init_local_exp_reldoc (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_local_exp_reldoc");

    if (UNDEF == (did_vec_inst = init_did_vec (spec, "doc.")))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_local_exp_reldoc");
    return (1);
}

int
local_exp_reldoc (unused, fdbk_info, inst)
char *unused;
FEEDBACK_INFO *fdbk_info;
int inst;
{
    long i, status;
    VEC dvec;

    PRINT_TRACE (2, print_string, "Trace: entering local_exp_reldoc");

    fdbk_info->num_occ = 0;
    fdbk_info->num_rel = 0;

    /* add the original query to list */
    if (UNDEF == add_doc (fdbk_info, fdbk_info->orig_query, 1))
        return (UNDEF);

    /* Add each rel retrieved doc to list */
    for (i = 0; i < fdbk_info->tr->num_tr; i++) {
        if (fdbk_info->tr->tr[i].rel == 1) {
	    /*
	     * In TREC routing, it is happening that a relevant
	     * document was discovered by another group and it
	     * has no index terms for Smart (see Q54, D779444)
	     * we don't want the program to die in such situations
	     */
            if (UNDEF == (status = did_vec (&(fdbk_info->tr->tr[i].did),
					    &dvec, did_vec_inst)))
                return (UNDEF);
	    if (status == 1) /* document vector was not empty */
		if (UNDEF == add_doc (fdbk_info, &dvec, 0))
		    return (UNDEF);
            fdbk_info->num_rel++;
        }
    }

    PRINT_TRACE (4, print_fdbk_info, fdbk_info);
    PRINT_TRACE (2, print_string, "Trace: leaving local_exp_reldoc");
    return (1);
}

int
close_local_exp_reldoc (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_local_exp_reldoc");

    if (UNDEF == close_did_vec (did_vec_inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_local_exp_reldoc");
    return (0);
}

static OCC *merge_buf;
static long num_merge = 0;

static int
add_doc (fdbk_info, dvec, orig_query_flag)
FEEDBACK_INFO *fdbk_info;
VEC *dvec;
int orig_query_flag;
{
    long ctype, old_occ_ctype, tmp;
    CON_WT *conwt, *end_conwt;
    OCC *new_occ, *old_occ, *end_old_occ, *tmp_occ;

    if (num_merge < fdbk_info->num_occ + dvec->num_conwt) {
	if (num_merge) Free(merge_buf);
        num_merge = fdbk_info->num_occ + dvec->num_conwt;
	if (NULL == (merge_buf = Malloc(num_merge, OCC)))
	    return(UNDEF);
    }
    (void) bzero ((char *) merge_buf, num_merge * sizeof(OCC));

    old_occ = fdbk_info->occ;
    end_old_occ = &fdbk_info->occ[fdbk_info->num_occ];
    /* Use old_occ_ctype == MAXLONG as sentinel.  */
    /* Set when reach end of old_occ */
    if (old_occ >= end_old_occ)
        old_occ_ctype = MAXLONG;
    else
        old_occ_ctype = old_occ->ctype;
    conwt = dvec->con_wtp;
    new_occ = merge_buf;

    for (ctype = 0; ctype < dvec->num_ctype; ctype++) {
        end_conwt = &conwt[dvec->ctype_len[ctype]];
        while (conwt < end_conwt) {
            if (ctype < old_occ_ctype ||
                (ctype == old_occ_ctype && conwt->con < old_occ->con)) {
                /* Add current document concept */
                new_occ->con = conwt->con;
                new_occ->ctype = ctype;
                if (orig_query_flag) {
                    new_occ->orig_weight = conwt->wt;
                    new_occ->query = 1;
                }
                else {
                    new_occ->rel_ret = 1;
                    new_occ->rel_weight = conwt->wt;
                }
                conwt++;
            }
            else if (ctype == old_occ_ctype && conwt->con == old_occ->con) {
                /* Merge old and new info  (Assume new doc not query */
                /* since query always done first) */
                new_occ->con = conwt->con;
                new_occ->ctype = ctype;
                new_occ->rel_ret = old_occ->rel_ret + 1;
                new_occ->rel_weight = old_occ->rel_weight + conwt->wt;
                new_occ->orig_weight = old_occ->orig_weight;
                new_occ->query = old_occ->query;
                conwt++;
                old_occ++;
                if (old_occ >= end_old_occ)
                    old_occ_ctype = MAXLONG;
                else
                    old_occ_ctype = old_occ->ctype;
            }
            else {
                /* Info from old occ comes next */
                new_occ->con = old_occ->con;
                new_occ->ctype = old_occ_ctype;
                new_occ->rel_ret = old_occ->rel_ret;
                new_occ->rel_weight = old_occ->rel_weight;
                new_occ->orig_weight = old_occ->orig_weight;
                new_occ->query = old_occ->query;
                old_occ++;
                if (old_occ >= end_old_occ)
                    old_occ_ctype = MAXLONG;
                else
                    old_occ_ctype = old_occ->ctype;
            }
            new_occ++;
        }
    }
    /* Finish up with old occ info */
    while (old_occ < end_old_occ) {
        new_occ->con = old_occ->con;
        new_occ->ctype = old_occ->ctype;
        new_occ->rel_ret = old_occ->rel_ret;
        new_occ->rel_weight = old_occ->rel_weight;
        new_occ->orig_weight = old_occ->orig_weight;
        new_occ->query = old_occ->query;
        old_occ++;
        new_occ++;
    }

    fdbk_info->num_occ = new_occ - merge_buf;
    /* Switch fdbk_info->occ, and merge_buf. Current fdbk_info->occ will 
     * be used for merging next time around. */
    tmp = fdbk_info->max_occ;
    fdbk_info->max_occ = num_merge;
    num_merge = tmp;
    tmp_occ = fdbk_info->occ;
    fdbk_info->occ = merge_buf;
    merge_buf = tmp_occ;
    return (1);
}
