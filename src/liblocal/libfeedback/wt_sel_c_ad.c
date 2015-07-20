#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libfeedback/wt_fdbk_sel_corr_adhoc.c,v 11.2 1993/02/03 16:50:03 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "feedback.h"


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Low-level rocchio feedback weighting procedure
 *1 local.feedback.weight.fdbk_sel
 *2 wt_fdbk_sel_corr_adhoc (unused1, fdbk_info, inst)
 *3   char *unused1;
 *3   FEEDBACK_INFO *fdbk_info;
 *3   int inst;
 *4 init_wt_fdbk_sel_corr_adhoc (spec, unused)
 *5   "feedback.alpha"
 *5   "feedback.beta"
 *5   "feedback.gamma"
 *5   "feedback.weight.trace"
 *4 close_wt_fdbk_sel_corr_adhoc (inst)
 *7 Same as feedback.weight.fdbk (libfeedback/wt_fdbk.c), except that
 *7 selection of ctype dependent num_expand terms occurs here. 
 *7 This routine expects the exitence of a text file in the format
 *7     qid ctype-2-con c_pair-with-rel c-in-rel c-in-nrel #-query-terms
 *7 WARNING: Assumes sorted by qid con.
 *7 Term selection based on final Rochhio weight.
***********************************************************************/

static float alpha;
static float beta;
static float gammavar;
static char *corr_file;
static SPEC_PARAM spec_args[] = {
    {"feedback.alpha",         getspec_float, (char *) &alpha},
    {"feedback.beta",          getspec_float, (char *) &beta},
    {"feedback.gamma",         getspec_float, (char *) &gammavar},
    {"feedback.corr_file",     getspec_dbfile,(char *) &corr_file},
    TRACE_PARAM ("feedback.weight.trace")
};
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static char *prefix;
static long num_expand; /* Number of terms to add */
static SPEC_PARAM_PREFIX prefix_spec_args[] = {
    {&prefix, "num_expand",       getspec_long, (char *) &num_expand}
};
static int num_prefix_spec_args =
    sizeof (prefix_spec_args) / sizeof (prefix_spec_args[0]);

static SPEC *saved_spec;

static int compare_con(), compare_qwt();

#define CHUNK 100000
typedef struct {
    long qid;
    long con;
    long common, count1, count2;
} CORR;

static CORR *corr;
static long num_corr;
static CORR *curr_corr;

int
init_wt_fdbk_sel_corr_adhoc (spec, unused)
SPEC *spec;
char *unused;
{
    FILE *fp;
    long qid, con, common, count1, count2;
    long max_corr;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_wt_fdbk_sel_corr_adhoc");

    if (NULL == (fp = fopen(corr_file, "r"))) {
        set_error(SM_ILLPA_ERR, "Can't open corr_file",
		  "init_wt_fdbk_sel_corr_adhoc");
        return UNDEF;
    }

    if (NULL == (corr = (CORR *) malloc(CHUNK*sizeof(CORR)))) {
        set_error(SM_ILLPA_ERR, "Not enough memory", "init_wt_fdbk_sel_corr_adhoc");
        return UNDEF;
    }

    max_corr = CHUNK;
    for (num_corr = 0; fscanf(fp, "%ld%ld%ld%ld%ld",
                              &qid, &con, &common, &count1, &count2) != EOF;
         num_corr++) {
        if (num_corr == max_corr) {
            max_corr += CHUNK;
            if (NULL == (corr = realloc((char *) corr,
                                        max_corr*sizeof(CORR)))) {
                set_error(SM_ILLPA_ERR, "Not enough memory",
                          "init_wt_fdbk_sel_corr_adhoc");
                return UNDEF;
            }
        }
        corr[num_corr].qid = qid;
        corr[num_corr].con = con;
        corr[num_corr].common = common;
        corr[num_corr].count1 = count1;
        corr[num_corr].count2 = count2;
    }
    curr_corr = corr;

    saved_spec = spec;

    PRINT_TRACE (2, print_string, "Trace: leaving init_wt_fdbk_sel_corr_adhoc");
    return (1);
}

int
wt_fdbk_sel_corr_adhoc (unused, fdbk_info, inst)
char *unused;
FEEDBACK_INFO *fdbk_info;
int inst;
{
    char param_prefix[PATH_LEN];
    long i, j;
    double rel_wt, nrel_wt, sum_old, sum_new;
    OCC *occp, *start_ctype, *end_occ;

    PRINT_TRACE (2, print_string, "Trace: entering wt_fdbk_sel_corr_adhoc");

    while (curr_corr - corr < num_corr &&
           curr_corr->qid < fdbk_info->orig_query->id_num.id)
        curr_corr++;

    for (i = 0; i < fdbk_info->num_occ; i++) {
        if (fdbk_info->num_rel > 0) 
            rel_wt = beta * fdbk_info->occ[i].rel_weight /
                fdbk_info->num_rel;
        else
            rel_wt = 0.0;

        if (fdbk_info->tr->num_tr > fdbk_info->num_rel) 
            nrel_wt = gammavar * fdbk_info->occ[i].nrel_weight /
                (fdbk_info->tr->num_tr - fdbk_info->num_rel);
        else
            nrel_wt = 0.0;

        fdbk_info->occ[i].weight = alpha * fdbk_info->occ[i].orig_weight + 
                                   rel_wt - nrel_wt;
        if (fdbk_info->occ[i].weight < 0.0)
            fdbk_info->occ[i].weight = 0.0;

        if (fdbk_info->occ[i].weight > 0.0 &&
            fdbk_info->occ[i].ctype == 2) {
            while (curr_corr - corr < num_corr &&
                   curr_corr->qid == fdbk_info->orig_query->id_num.id &&
                   curr_corr->con < fdbk_info->occ[i].con)
                curr_corr++;
            assert(curr_corr - corr < num_corr &&
                   curr_corr->qid == fdbk_info->orig_query->id_num.id &&
                   curr_corr->con == fdbk_info->occ[i].con);
	    /* Coocc wt = Rochhio wt * (1 - MAX( P(ti|tj), P(tj|ti) )) */
            fdbk_info->occ[i].weight *= 1 - 
		(float)curr_corr->common /
		MIN((float)curr_corr->count1, (float)curr_corr->count2);
	    if (fdbk_info->occ[i].weight < 0.0)
		fdbk_info->occ[i].weight = 0;
        }
    }

    /* keep all query terms and num_expand terms for each ctype */
    start_ctype = occp = fdbk_info->occ;
    end_occ = fdbk_info->occ + fdbk_info->num_occ;
    for (i = 0; i <= (end_occ - 1)->ctype; i++) {
	(void) sprintf (param_prefix, "local.feedback.ctype.%ld.", i);
	prefix = param_prefix;
	if (UNDEF == lookup_spec_prefix(saved_spec,
					&prefix_spec_args[0],
					num_prefix_spec_args))
	    return (UNDEF);
	while (occp < end_occ && occp->ctype == i) 
	    occp++;

	/* sort terms by <query, weight> */
	qsort((char *) start_ctype, occp - start_ctype, sizeof(OCC), 
	      compare_qwt);

	/* keep the best num_expand terms; zero out rest */
	for (j = 0; j < occp - start_ctype && start_ctype[j].query; j++);
	for (j += num_expand; j < occp - start_ctype; j++)
	    start_ctype[j].weight = 0.0;
	/* sort back by con */
	qsort((char *) start_ctype, occp - start_ctype, sizeof(OCC), 
	      compare_con);
	start_ctype = occp;
    }

    /* scale final wts to ensure compatibility with original query wts */
    sum_old = 0;
    for (i = 0; i < fdbk_info->orig_query->num_conwt; i++)
	sum_old += fdbk_info->orig_query->con_wtp[i].wt;
    assert(sum_old > 0);
    sum_new = 0;
    for (i = 0; i < fdbk_info->num_occ; i++) 
	sum_new += fdbk_info->occ[i].weight;
    for (i = 0; i < fdbk_info->num_occ; i++) 
	fdbk_info->occ[i].weight *= sum_old/sum_new;

    PRINT_TRACE (4, print_fdbk_info, fdbk_info);
    PRINT_TRACE (2, print_string, "Trace: leaving wt_fdbk_sel_corr_adhoc");

    return (1);
}

int
close_wt_fdbk_sel_corr_adhoc (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering/leaving close_wt_fdbk_sel_corr_adhoc");
    return (0);
}

static int
compare_con (ptr1, ptr2)
OCC *ptr1;
OCC *ptr2;
{
    if (ptr1->con < ptr2->con)
        return (-1);
    if (ptr1->con > ptr2->con)
        return (1);
    return (0);
}

/* Terms sorted by final weight */
static int
compare_qwt (ptr1, ptr2)
OCC *ptr1;
OCC *ptr2;
{
    if (ptr1->query && !ptr2->query)	
	return (-1);
    if (ptr2->query && !ptr1->query)	
	return (1);
    if (ptr1->weight > ptr2->weight)
        return (-1);
    if (ptr1->weight < ptr2->weight)
        return (1);
    return (0);
}
