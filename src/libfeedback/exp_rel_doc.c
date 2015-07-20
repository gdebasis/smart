#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfeedback/exp_rel_doc.c,v 11.0 1992/07/21 18:20:39 chrisb Exp $";
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
init_exp_rel_doc (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_exp_rel_doc");

    if (UNDEF == (did_vec_inst = init_did_vec (spec, (char *) NULL)))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_exp_rel_doc");
    return (1);
}

int
exp_rel_doc (unused, fdbk_info, inst)
char *unused;
FEEDBACK_INFO *fdbk_info;
int inst;
{
    long i, status;
    VEC dvec;

    PRINT_TRACE (2, print_string, "Trace: entering exp_rel_doc");

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
    PRINT_TRACE (2, print_string, "Trace: leaving exp_rel_doc");
    return (1);
}


int
close_exp_rel_doc (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_exp_rel_doc");

    if (UNDEF == close_did_vec (did_vec_inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_exp_rel_doc");
    return (0);
}

static OCC *merge1;
static long num_merge1 = 0;
static OCC *merge2;
static long num_merge2 = 0;


static int
add_doc (fdbk_info, dvec, orig_query_flag)
FEEDBACK_INFO *fdbk_info;
VEC *dvec;
int orig_query_flag;
{
    CON_WT *conwt, *end_conwt;
    long ctype;

    OCC *new_occ;
    OCC *old_occ;
    OCC *end_old_occ;

    long old_occ_ctype;

    old_occ = fdbk_info->occ;
    end_old_occ = &fdbk_info->occ[fdbk_info->num_occ];
    if (fdbk_info->occ == merge1) {
        /* New occ will be merge2 */
        if (num_merge2 < fdbk_info->num_occ + dvec->num_conwt) {
            /* Must reserve more space for occ info */
            if (num_merge2 > 0)
                (void) free ((char *) merge2);
            if (NULL == (merge2 = (OCC *)
                         malloc ((unsigned) (fdbk_info->num_occ +
                                             dvec->num_conwt) * sizeof (OCC))))
                return (UNDEF);
        }
        num_merge2 = fdbk_info->num_occ + dvec->num_conwt;
        new_occ = merge2;
    }
    else {
        /* New occ will be merge1 */
        if (num_merge1 < fdbk_info->num_occ + dvec->num_conwt) {
            /* Must reserve more space for occ info */
            if (num_merge1 > 0)
                (void) free ((char *) merge1);
            if (NULL == (merge1 = (OCC *)
                         malloc ((unsigned) (fdbk_info->num_occ +
                                             dvec->num_conwt) * sizeof (OCC))))
                return (UNDEF);
        }
        num_merge1 = fdbk_info->num_occ + dvec->num_conwt;
        new_occ = merge1;
    }

    fdbk_info->occ = new_occ;

    (void) bzero ((char *) new_occ,
                  (fdbk_info->num_occ + dvec->num_conwt) * sizeof (OCC));

    /* Use old_occ_ctype == MAXLONG as sentinel.  */
    /* Set when reach end of old_occ */
    if (old_occ >= end_old_occ)
        old_occ_ctype = MAXLONG;
    else
        old_occ_ctype = old_occ->ctype;

    conwt = dvec->con_wtp;
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

    fdbk_info->num_occ = new_occ - fdbk_info->occ;

    return (1);
}

