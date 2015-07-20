#ifdef RCSID
static char rcsid[] = "$Header: v 11.2 1993/02/03 16:49:59 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/


#include "common.h"
#include "docindex.h"
#include "functions.h"
#include "io.h"
#include "local_fdbk.h"
#include "param.h"
#include "proc.h"
#include "proximity.h"
#include "retrieve.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "vector.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("feedback.expand.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

/*
 * We have a vector for each term in the original query. The concepts in
 * this vector are the terms that co-occur near the query terms in the
 * retrieved documents and their weights represent the "degree of
 * local cooccurrence" (based on say distance in the text).
 *
 * WE ONLY CONSIDER COOCCURRENCES WITH SINGLE TERM as cooccurrences with
 * phrases just duplicate what has been captured by single terms.
 *
 */
/* This version computes proximities for original query terms rather 
 * than for all the single terms selected for expansion.
 */

static long condoc_inst, add_inst, wt_inst;
static long max_qterms;
static PROXIMITY_LIST rel_prox, nrel_prox;

static int make_stats();
int init_get_condoc(), get_condoc(), close_get_condoc();
int init_add_prox(), add_prox(), close_add_prox();
int init_weight_prox(), weight_prox(), close_weight_prox();


int
init_exp_prox (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
	return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_exp_prox");

    /* Call all initialization procedures */
    if (UNDEF == (condoc_inst = init_get_condoc(spec, NULL)) ||
        UNDEF == (add_inst = init_add_prox(spec, NULL)) ||
        UNDEF == (wt_inst = init_weight_prox(spec, NULL)))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_prox");
    return (0);
}


int 
exp_prox (unused, fdbk_info, inst)
char *unused;
FEEDBACK_INFO *fdbk_info;
int inst;
{
    long num_qterms, i;
    SM_CONDOC condoc;
    TR_VEC *tr;
    PROXIMITY prox, *relp, *nrelp;
    LOCAL_FDBK_INFO lfdbk_info;

    PRINT_TRACE (2, print_string, "Trace: entering exp_prox");

    tr = fdbk_info->tr;
    if (tr->num_tr == 0)
        return (0);

    lfdbk_info.fdbk_info = fdbk_info;

    /*
     * Make sure that you have enough space to store proximities
     * for every ctype 0 query term.
     */
    num_qterms = fdbk_info->orig_query->ctype_len[0]; 
    if (max_qterms < num_qterms) {
        if (max_qterms) {
	    Free(rel_prox.prox);
	    Free(nrel_prox.prox);
	}
	max_qterms = num_qterms;
	if (NULL == (rel_prox.prox = Malloc(max_qterms, PROXIMITY)) ||
	    NULL == (nrel_prox.prox = Malloc(max_qterms, PROXIMITY)))
	    return UNDEF;
    }

    /* Initialise */
    relp = rel_prox.prox; nrelp = nrel_prox.prox;
    for (i = 0; i < num_qterms; i++) {
	relp[i].con = nrelp[i].con = fdbk_info->orig_query->con_wtp[i].con;
	relp[i].num_occ = nrelp[i].num_occ = 0;
	relp[i].num_doc = nrelp[i].num_doc = 0;
	relp[i].num_ctype = nrelp[i].num_ctype = 0;
	relp[i].num_conwt = nrelp[i].num_conwt = 0;
	relp[i].ctype_len = nrelp[i].ctype_len = (long *) NULL;
	relp[i].con_wtp = nrelp[i].con_wtp = (PROX_WT *) NULL;
    }
    rel_prox.num_prox = num_qterms;
    nrel_prox.num_prox = num_qterms;
    lfdbk_info.prox = &prox;

    /* Collect information for all rel docs. */
    /* main loop that collects proximities document by document */
    for (i = 0; i < tr->num_tr; i++) {
	if (tr->tr[i].rel) {
	    if (UNDEF == get_condoc(&tr->tr[i].did, &condoc, condoc_inst) ||
		UNDEF == add_prox(&condoc, &rel_prox, add_inst))
		return UNDEF;
	}
    }
    /* weight the proximities and fill in fdbk_info with rel info */
    if (UNDEF == weight_prox(&rel_prox, &lfdbk_info, wt_inst))
	return(UNDEF);
    if (UNDEF == make_stats(&prox, fdbk_info))
	return(UNDEF);

    /* Collect information for top 2R non-rel docs. */
    /* - Deleted for adhoc expansion */

    /* Free up space */
    for (i=0; i < num_qterms; i++) {
	Free(relp[i].con_wtp);
	Free(nrelp[i].con_wtp);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving exp_prox");
    return (1);
}


int
close_exp_prox (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_exp_prox");

    /* Call all close procedures */
    if (UNDEF == close_get_condoc(condoc_inst) ||
        UNDEF == close_add_prox(add_inst) ||
        UNDEF == close_weight_prox(wt_inst)) 
        return (UNDEF);
    
    if (max_qterms) {
	Free(rel_prox.prox);
	Free(nrel_prox.prox);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_exp_prox");
    return (0);
}


/* Fill in info for rel docs */
static int
make_stats(prox, fdbk_info)
PROXIMITY *prox;
FEEDBACK_INFO *fdbk_info;
{
    long i;
    PROX_WT *wtp;
    OCC *occp;

    if (fdbk_info->max_occ < fdbk_info->num_occ + prox->num_conwt) {
	fdbk_info->max_occ = fdbk_info->num_occ + prox->num_conwt;
        if (NULL == (fdbk_info->occ = 
		     Realloc(fdbk_info->occ, fdbk_info->max_occ, OCC)))
            return (UNDEF);
    }

    wtp = prox->con_wtp;
    occp = fdbk_info->occ + fdbk_info->num_occ;

    for (i = 0; i < prox->num_conwt; i++, wtp++) {
	occp->con = wtp->con;
	occp->ctype = 2;
	occp->query = 0;
	occp->rel_ret = wtp->num_doc;
	occp->nrel_ret = occp->nret = 0;
	occp->weight = occp->orig_weight = 0;
	occp->rel_weight = wtp->wt;
	occp->nrel_weight = 0.0;
	occp++;
    }
    fdbk_info->num_occ += occp - (fdbk_info->occ + fdbk_info->num_occ);

    return(1);
}
