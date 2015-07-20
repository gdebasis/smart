#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfeedback/exp_nr_co.c,v 11.2 1993/02/03 16:49:57 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "feedback.h"
#include "functions.h"
#include "inv.h"
#include "param.h"
#include "spec.h"
#include "tr_vec.h"
#include "trace.h"
#include "proc.h"


/* Expand query by adding all terms occurring non-randomly in rel docs.
 * Query terms are always in the new query.
 * Handles single terms (word, ctype 0), phrases (phrase, ctype 1), 
 * cooccurring word pairs (coocc, ctype 2).
 * Word pairs occurring close to each other (prox, ctype 3) too b* trouble-
 * some, eliminated.
 */

static float alpha, beta, gammavar;
static long num_expand;	/* Number of terms to add */
static char *inv_file;           
static long inv_file_mode;
static PROC_TAB *exp_coocc;

static SPEC_PARAM spec_args[] = {
    {"feedback.alpha",		getspec_float,	(char *) &alpha},
    {"feedback.beta",		getspec_float,	(char *) &beta},
    {"feedback.gamma",		getspec_float,	(char *) &gammavar},
    {"feedback.ctype.0.num_for_coocc", getspec_long, (char *) &num_expand},
    {"feedback.ctype.0.inv_file", getspec_dbfile, (char *) &inv_file},
    {"feedback.ctype.0.inv_file.rmode", getspec_filemode, (char *) &inv_file_mode},
    {"feedback.exp_coocc",      getspec_func,   (char *) &exp_coocc},
    TRACE_PARAM ("feedback.expand.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static char *prefix;
static long min_percent;	/* Cutoff for random occurrences */
static SPEC_PARAM_PREFIX prefix_spec_args[] = {
    {&prefix,	"min_percent",	getspec_long,	(char *) &min_percent},
};
static int num_prefix_spec_args = 
	sizeof (prefix_spec_args) / sizeof (prefix_spec_args[0]);

static int inv_fd;
static int exp_rel_doc_inst, exp_coocc_inst;
static SPEC *save_spec;

static void get_nrel_wt();
static int compare_con(), compare_wt(), compare_occ_info();
int init_local_exp_reldoc(), local_exp_reldoc(), close_local_exp_reldoc();


int
init_exp_nonrand_co (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_exp_nonrand_co");

    if (UNDEF == (inv_fd = open_inv(inv_file, inv_file_mode)))
        return (UNDEF);

    if (UNDEF == (exp_rel_doc_inst = init_local_exp_reldoc(spec, (char *) NULL)) ||
	UNDEF == (exp_coocc_inst = exp_coocc->init_proc(spec, (char *) NULL)))
        return (UNDEF);

    save_spec = spec;

    PRINT_TRACE (2, print_string, "Trace: leaving init_exp_nonrand_co");
    return (1);
}


int
exp_nonrand_co (unused, fdbk_info, inst)
char *unused;
FEEDBACK_INFO *fdbk_info;
int inst;
{
    char param_prefix[40];
    long num_orig_query, num_this_ctype, min_rel;
    long num_words, i, j;
    double inv_num_rel, inv_num_nrel, rel_wt, nrel_wt;
    OCC *start_ctype_occ, *end_ctype_occ, *end_occ, *end_good_occ, *occp;

    PRINT_TRACE (2, print_string, "Trace: entering exp_nonrand_co");

    /* Get all terms (ctypes 0 and 1) occurring in rel docs */
    if (UNDEF == local_exp_reldoc (unused, fdbk_info, exp_rel_doc_inst))
        return (UNDEF);

    if (fdbk_info->num_rel > 0) {
        PRINT_TRACE (8, print_fdbk_info, fdbk_info);

        /* Go through expanded terms ctype by ctype keeping all "non-randomly
	 * occurring" terms */
        start_ctype_occ = fdbk_info->occ;
        end_good_occ = fdbk_info->occ;
        end_occ = &fdbk_info->occ[fdbk_info->num_occ];
        while (start_ctype_occ < end_occ) {
            end_ctype_occ = start_ctype_occ + 1;
            num_orig_query = (start_ctype_occ->query ? 1 : 0);
            while (end_ctype_occ < end_occ &&
                   end_ctype_occ->ctype == start_ctype_occ->ctype) {
                if (end_ctype_occ->query)
                    num_orig_query++;
                end_ctype_occ++;
            }

            num_this_ctype = end_ctype_occ - start_ctype_occ;
            /* Sort the occurrence info by
             *  1. query (all original query terms occur first)
             *  2. rel_ret (those terms occurring in most rel docs occur next)
             */
            qsort((char *) start_ctype_occ, num_this_ctype, sizeof (OCC),
		  compare_occ_info);

            sprintf (param_prefix, "feedback.ctype.%ld.",
		     start_ctype_occ->ctype);
            prefix = param_prefix;
            if (UNDEF == lookup_spec_prefix (save_spec,
                                             &prefix_spec_args[0],
                                             num_prefix_spec_args))
                return (UNDEF);
	    min_rel = (long) 
		floor((double)(fdbk_info->num_rel * min_percent)/100.0 + 0.5);

	    /* Ignore terms which occur in < min_percent of the rel. docs */
	    occp = start_ctype_occ + num_orig_query;
	    while (occp < end_ctype_occ && occp->rel_ret >= min_rel)
		occp++;
	    num_this_ctype = occp - start_ctype_occ;
            /* Resort by con */
            qsort((char *) start_ctype_occ, num_this_ctype, sizeof(OCC), 
		  compare_con);
            /* Copy into good portion of occ array (skip copying if already
               in correct location) */
            if (start_ctype_occ != end_good_occ) {
                (void) bcopy ((char *) start_ctype_occ,
                              (char *) end_good_occ,
                              sizeof (OCC) * (int) num_this_ctype);
            }
            end_good_occ += num_this_ctype;
            start_ctype_occ = end_ctype_occ;
        }
        fdbk_info->num_occ = end_good_occ - fdbk_info->occ;

	/* Get the ctype 0 query terms that will be selected for the final
	 * feedback query. These are used by exp_coocc */
	inv_num_rel = 1.0 / (float) fdbk_info->num_rel;
	if (fdbk_info->tr->num_tr > fdbk_info->num_rel) 
	    inv_num_nrel = 1.0 / (float)(fdbk_info->tr->num_tr -
					 fdbk_info->num_rel);
	else inv_num_nrel = 0.0;

	for (i = 0; i < fdbk_info->num_occ; i++) {
	    if (fdbk_info->occ[i].ctype > 0) break;
	    rel_wt = beta * fdbk_info->occ[i].rel_weight * inv_num_rel;
	    get_nrel_wt(fdbk_info, i);
	    nrel_wt = gammavar * fdbk_info->occ[i].nrel_weight * inv_num_nrel;
	    fdbk_info->occ[i].weight = alpha*fdbk_info->occ[i].orig_weight + 
		rel_wt - nrel_wt;
	    if (fdbk_info->occ[i].weight < 0.0)
		fdbk_info->occ[i].weight = 0.0;
	}
	num_words = i;

	/* sort terms by final weight; keep best num_expand, zero out rest */
	qsort((char *) fdbk_info->occ, num_words, sizeof(OCC), compare_wt);
	for (j = num_expand; j < num_words; j++)
	    fdbk_info->occ[j].weight = 0.0;
	/* sort back by con */
	qsort((char *) fdbk_info->occ, num_words, sizeof(OCC), compare_con);

	/* Now, get cooccs (ctype 2) */
	if (UNDEF == exp_coocc->proc(unused, fdbk_info, exp_coocc_inst))
	    return (UNDEF);
    }

    PRINT_TRACE (4, print_fdbk_info, fdbk_info);
    PRINT_TRACE (2, print_string, "Trace: leaving exp_nonrand_co");
    return (1);
}


int
close_exp_nonrand_co (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_exp_nonrand_co");

    if (UNDEF == close_local_exp_reldoc(exp_rel_doc_inst) ||
	UNDEF == exp_coocc->close_proc(exp_coocc_inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_exp_nonrand_co");
    return (0);
}


static void
get_nrel_wt(fdbk_info, index)
FEEDBACK_INFO *fdbk_info;
long index;
{
    long tr_ind, k;
    INV inv;

    inv.id_num = fdbk_info->occ[index].con;
    if (1 != seek_inv(inv_fd, &inv) ||
	1 != read_inv(inv_fd, &inv) ) {
        PRINT_TRACE (1, print_string, "exp_nr_co: concept not found in inv_file:");
        PRINT_TRACE (1, print_long, &fdbk_info->occ[index].con);
	return;
    }

    tr_ind = 0;    
    for (k = 0; k < inv.num_listwt; k++) {
	/* Discover whether doc is in top_docs */
	/* Assume both inverted list and tr are sorted */
	while (tr_ind < fdbk_info->tr->num_tr &&
	       fdbk_info->tr->tr[tr_ind].did < inv.listwt[k].list)
	    tr_ind++;

	if (tr_ind < fdbk_info->tr->num_tr &&
	    fdbk_info->tr->tr[tr_ind].did == inv.listwt[k].list) {
	    if (! fdbk_info->tr->tr[tr_ind].rel) {
		fdbk_info->occ[index].nrel_ret++;
		fdbk_info->occ[index].nrel_weight += inv.listwt[k].wt;
	    }
	}
	else fdbk_info->occ[index].nret++;
    }

    return;
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
compare_wt (ptr1, ptr2)
OCC *ptr1;
OCC *ptr2;
{
    if (ptr1->weight > ptr2->weight)
        return (-1);
    if (ptr1->weight < ptr2->weight)
        return (1);
    return (0);
}

static int
compare_occ_info (ptr1, ptr2)
OCC *ptr1;
OCC *ptr2;
{
    if (ptr1->query) {
        if (ptr2->query)
            return (0);
        return (-1);
    }
    if (ptr2->query)
        return (1);
    if (ptr1->rel_ret > ptr2->rel_ret)
        return (-1);
    if (ptr1->rel_ret < ptr2->rel_ret)
        return (1);
    return (0);
}
