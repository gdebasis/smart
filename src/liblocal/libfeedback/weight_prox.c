#ifdef RCSID
static char rcsid[] = "$Header: v 11.2 1993/02/03 16:49:59 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/* Original program (in smart.mitra) modified to return the best
 * proximities.
 * For each term, a proximity weight is calculated over all query 
 * terms.
 * ONLY SINGLE-TERM PAIRS considered.
 */

#include "common.h"
#include "context.h"
#include "docdesc.h"
#include "docindex.h"
#include "functions.h"
#include "io.h"
#include "local_fdbk.h"
#include "param.h"
#include "proc.h"
#include "proximity.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "vector.h"

static float min_percent;
static SPEC_PARAM spec_args[] = {
    {"feedback.prox.min_percent", getspec_float, (char *) &min_percent},
    TRACE_PARAM ("feedback.weight_prox.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static long idf_inst;
static long ctype_len;
static PROX_WT *all_prox_wt;
static long max_all_prox_wt;

static int query_term();
static int compare_con();


int
init_weight_prox (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
	return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_weight_prox");

    if (UNDEF == (idf_inst = init_con_cw_idf(spec, "index.ctype.0.")))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_weight_prox");
    return (0);
}


int 
weight_prox (prox_list, lfdbk_info, inst)
PROXIMITY_LIST *prox_list;
LOCAL_FDBK_INFO *lfdbk_info;
int inst;
{
    long min_rel, num_reqd, i, j;
    float idf;
    PROX_WT *ptr, *cwp;

    PRINT_TRACE (2, print_string, "Trace: entering weight_prox");

    lfdbk_info->prox->num_ctype = 1;
    lfdbk_info->prox->num_conwt = 0;
    lfdbk_info->prox->ctype_len = &ctype_len;

    num_reqd = 0;
    for (i = 0; i < prox_list->num_prox; i++)
	num_reqd += prox_list->prox[i].num_conwt;
    if (max_all_prox_wt < num_reqd) {
	if (max_all_prox_wt) Free(all_prox_wt);
	max_all_prox_wt += num_reqd;
	if (NULL == (all_prox_wt = Malloc(max_all_prox_wt, PROX_WT)))
	    return(UNDEF);
    }
    lfdbk_info->prox->con_wtp = all_prox_wt;

    min_rel = 
	(long) floor((double)(min_percent/100.0 * 
			      lfdbk_info->fdbk_info->num_rel) + 0.5);
    min_rel = MAX(min_rel, 2);

    /* Eliminate random proximities and proximities involving 2 query 
     * terms (including self-referential ones).
     */
    ptr = all_prox_wt;
    for (j = 0; j < prox_list->num_prox; j++) {
	cwp = prox_list->prox[j].con_wtp;
	for (i = 0; i < prox_list->prox[j].num_conwt; i++) {
	    if (cwp[i].num_doc >= min_rel &&		/* non-random */
		!query_term(lfdbk_info, cwp[i].con)) {	/* non-qterm */
		*ptr = cwp[i];
		if (UNDEF == con_cw_idf(&ptr->con, &idf, idf_inst))
		    return (UNDEF);
		ptr->wt *= lfdbk_info->fdbk_info->orig_query->con_wtp[j].wt *
		           idf / prox_list->prox[j].num_doc;
		ptr++;
	    }
	}
    }

    /* For each term, combine proximities over all query terms */
    qsort((char *)all_prox_wt, ptr - all_prox_wt, sizeof(PROX_WT),
	  compare_con);
    all_prox_wt[0].num_terms = 1;
    all_prox_wt[0].num_doc = -1;
    for (i = 0, j = 1; j < ptr - all_prox_wt; j++) {
	if (all_prox_wt[j].con != all_prox_wt[i].con) {
	    all_prox_wt[++i] = all_prox_wt[j];
	    all_prox_wt[i].num_doc = -1;
	    all_prox_wt[i].num_terms = 1;
	}
	else {
	    all_prox_wt[i].num_terms++;
	    all_prox_wt[i].wt += all_prox_wt[j].wt;
	}
    }

    lfdbk_info->prox->ctype_len[0] = i + 1;
    lfdbk_info->prox->num_conwt = i + 1;

    PRINT_TRACE (2, print_string, "Trace: leaving weight_prox");
    return (1);
}


int
close_weight_prox (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_weight_prox");

    if (UNDEF == close_con_cw_idf(idf_inst))
	return(UNDEF);

    if (max_all_prox_wt) Free(all_prox_wt);

    PRINT_TRACE (2, print_string, "Trace: leaving close_weight_prox");
    return (0);
}


static int
query_term(lfdbk_info, con)
LOCAL_FDBK_INFO *lfdbk_info;
long con;
{
    long i;
    VEC *qvec = lfdbk_info->fdbk_info->orig_query;

    for (i = 0; i < qvec->ctype_len[0]; i++)
	if (qvec->con_wtp[i].con == con)
	    return(1);

    return(0);
}


static int
compare_con(c1, c2)
PROX_WT *c1;
PROX_WT *c2;
{ 
    return(c1->con - c2->con);
}
