#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libfeedback/exp.c,v 11.2 1993/02/03 16:50:03 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "context.h"
#include "feedback.h"
#include "functions.h"
#include "inv.h"
#include "param.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "proc.h"

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Low-level rocchio feedback expansion procedure
 *2 exp_corr_notok (unused1, fdbk_info, inst)
 *3   char *unused1;
 *3   FEEDBACK_INFO *fdbk_info;
 *3   int inst;
 *4 init_exp_corr_notok (spec, unused)
 *4 close_exp_corr_notok (inst)
 *7 fdbk_info brings in information about ctypes 0 and 1.
 *7 Information about word-pairs (coocc, ctype 2) is filled in.
 *7 The inv_files should be Lnu weighted. Idf for cooccs is calculated
 *7 at run-time.
***********************************************************************/

static char *inv_file;           
static long inv_file_mode;
static PROC_TAB *contok;
static long min_percent;
static char *textloc_file;

static SPEC_PARAM spec_args[] = {
    {"feedback.coocc.ctype.0.inv_file", getspec_dbfile, (char *) &inv_file},
    {"feedback.coocc.ctype.0.inv_file.rmode", getspec_filemode, (char *) &inv_file_mode},
    {"feedback.coocc.ctype.0.con_to_token", getspec_func, (char *) &contok},
    {"feedback.coocc.min_percent", getspec_long, (char *) &min_percent},
    {"feedback.textloc_file",	getspec_dbfile, (char *) &textloc_file},
    TRACE_PARAM ("feedback.corr.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int inv_fd;
static float num_docs;
static long contok_inst, tokcon_inst;
static long max_words = 0;
static INV *inv_list;
static OCC *occ;
static SPEC *saved_spec;

static int merge_stats(), gen_coocc();
static int compare_occ();
int init_tokcon_dict_niss(), tokcon_dict_niss(), close_tokcon_dict_niss();

#define MAX_DOCS 5120

int
init_exp_corr_notok (spec, unused)
SPEC *spec;
char *unused;
{
    CONTEXT old_context;
    REL_HEADER *rh = NULL;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_exp_corr_notok");

    if (UNDEF == (inv_fd = open_inv(inv_file, inv_file_mode)))
	return (UNDEF);

    /* Find number of docs. for idf computation for term-pairs */
    if (!VALID_FILE (textloc_file) ||
        NULL == (rh = get_rel_header (textloc_file))) {
	set_error (SM_ILLPA_ERR, textloc_file, "exp_corr_notok");
	return (UNDEF);
    }
    num_docs = (float) (rh->num_entries + 1);

    if (UNDEF == (contok_inst = contok->init_proc(spec, "ctype.0.")))
        return(UNDEF);

    old_context = get_context();
    set_context (CTXT_INDEXING_DOC);
    if (UNDEF == (tokcon_inst = init_tokcon_dict_niss(spec, "coocc.")))
	return(UNDEF);
    set_context (old_context);

    saved_spec = spec;

    PRINT_TRACE (2, print_string, "Trace: leaving init_exp_corr_notok");
    return (1);
}

int
exp_corr_notok (unused, fdbk_info, inst)
char *unused;
FEEDBACK_INFO *fdbk_info;
int inst;
{
    FEEDBACK_INFO coocc_info;	/* coocc stats for this query */

    PRINT_TRACE (2, print_string, "Trace: entering exp_corr_notok");

    /* Generate coocc stats for each pair of single query terms */
    if (UNDEF == gen_coocc(fdbk_info, &coocc_info))
	return(UNDEF);

    /* Merge in the stats with the original fdbk_info */
    if (UNDEF == merge_stats(&coocc_info, fdbk_info))
	return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving exp_corr_notok");

    return (1);
}

int
close_exp_corr_notok (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_exp_corr_notok");

    if (UNDEF == close_inv(inv_fd))
	return (UNDEF);

    if (UNDEF == contok->close_proc(contok_inst) ||
	UNDEF == close_tokcon_dict_niss(tokcon_inst))
	return(UNDEF);

    if (max_words) {
	Free(inv_list);
	Free(occ);
    }

    PRINT_TRACE (2, print_string, "Trace: entering close_exp_corr_notok");
    return (0);
}

/**************************************************************************
 * Constructs a vector for the expanded query and gives each term a weight 
 * equal to the coocc factor computed for that term with all other query 
 * terms.
 * Similar to form_query.
 **************************************************************************/
static int
gen_coocc(fdbk_info, coocc_info)
FEEDBACK_INFO *fdbk_info;
FEEDBACK_INFO *coocc_info;
{
    char token_buf[PATH_LEN], *term1, *term2;
    long num_words, num_rel, num_ret, rel_count, nrel_count, min_rel;
    long co_df, tr_ind, i, j;
    float rel_wt, nrel_wt, co_idf;
    LISTWT *lwp1, *lwp2;
    INV inv1, inv2, *invp;
    OCC *occp;

    long ret1=0, ret2=0;
    float rel_corr, nrel_corr, nret_corr, nrelall_corr;

    /* Count single terms */
    for (i = 0; i < fdbk_info->num_occ && fdbk_info->occ[i].ctype == 0; i++);
    num_words = i;

    if (fdbk_info->num_occ > max_words) {
	if (max_words) {
	    Free(inv_list); 
	    Free(occ);
	}
	max_words += fdbk_info->num_occ;
	if (NULL == (inv_list = Malloc(max_words, INV)) ||
	    NULL == (occ = Malloc((max_words * (max_words - 1))/2, OCC)))
	    return(UNDEF);
    }

    for (i = 0; i < fdbk_info->num_occ; i++) {
        invp = &inv_list[i];
	if (fdbk_info->occ[i].ctype == 0 && fdbk_info->occ[i].weight > 0.0) {
	    invp->id_num = fdbk_info->occ[i].con;
	    if (1 != seek_inv(inv_fd, invp) ||
		1 != read_inv(inv_fd, invp)) {
                PRINT_TRACE (1, print_string, "exp_corr_notok: concept not found in inv_file:");
                PRINT_TRACE (1, print_long, &invp->id_num);
		continue;
	    }
	}
        else {
            invp->id_num = -1;
        }
    }

    /* Gather cooccurrence stats for each pair of query terms in coocc_info */
    /* Only single-term pairs considered */
    min_rel = (long) 
	floor((double)(fdbk_info->num_rel * min_percent)/100.0 + 0.5);
    occp = occ;
    for (i = 0; i < fdbk_info->num_occ; i++) {
        if (inv_list[i].id_num == -1)
            continue;
	/* Get inv list for 1st */
	inv1 = inv_list[i];
	for (j = i+1; j < fdbk_info->num_occ; j++) { 
            if (inv_list[j].id_num == -1)
                continue;
	    inv2 = inv_list[j];

	    lwp1 = inv1.listwt;
	    lwp2 = inv2.listwt;
	    co_df = 0; tr_ind = 0; 
	    rel_count = nrel_count = 0;
	    rel_wt = nrel_wt = 0;

	    /* Find common docs. that are in top-retrieved set */
	    while (lwp1 < inv1.listwt + inv1.num_listwt &&
		   lwp2 < inv2.listwt + inv2.num_listwt) {
		if (lwp1->list < lwp2->list) lwp1++;
		else if (lwp1->list > lwp2->list) lwp2++;
		else {		    
		    co_df++;
		    /* Find whether doc is in top_docs */
		    /* Assume both inverted list and tr are sorted */
		    /* Wt for rel docs is +ve, wt for non-rel docs is -ve */
		    while (tr_ind < fdbk_info->tr->num_tr &&
			   fdbk_info->tr->tr[tr_ind].did < lwp1->list)
			tr_ind++;
		    if (tr_ind < fdbk_info->tr->num_tr &&
			fdbk_info->tr->tr[tr_ind].did == lwp1->list) {
			if (fdbk_info->tr->tr[tr_ind].rel) {
			    rel_count++; 
			    rel_wt += MIN(lwp1->wt, lwp2->wt);
			}
			else {
			    nrel_count++; 
			    nrel_wt += MIN(lwp1->wt, lwp2->wt);
			}
		    }
		    lwp1++; lwp2++;
		}
	    }

	    if (rel_count < min_rel) continue;
	    if (UNDEF == contok->proc(&(inv1.id_num), &term1, contok_inst) ||
		UNDEF == contok->proc(&(inv2.id_num), &term2, contok_inst))
		return(UNDEF);
	    sprintf(token_buf, "%s %s", term1, term2);
	    if (UNDEF == tokcon_dict_niss(token_buf, &occp->con, tokcon_inst))
		return(UNDEF);

	    occp->ctype = 2;
	    occp->query = 0;
	    occp->rel_ret = rel_count;
	    occp->nrel_ret = nrel_count;
	    occp->nret = co_df - rel_count - nrel_count;
	    occp->weight = occp->orig_weight = 0;
	    /* get idf weights */
	    co_idf = log(num_docs/(float)co_df);
	    occp->rel_weight = rel_wt * co_idf;
	    occp->nrel_weight = nrel_wt * co_idf;

            ret1 = fdbk_info->occ[i].rel_ret + fdbk_info->occ[i].nrel_ret;
            ret2 = fdbk_info->occ[j].rel_ret + fdbk_info->occ[j].nrel_ret;
            num_rel = fdbk_info->num_rel;
            num_ret = fdbk_info->tr->num_tr;

            /* Correlation is (P12 - P1P2) / sqrt (P1(1-P1)P2(1-P2)) */
#define CORREL(p12,p1,p2) (p1==1.0 || p2==1.0 || p1==0.0 || p2==0.0 ? 0.0: \
			   (p12 - p1 * p2) / \
			   sqrt ((double)(p1*(1.0 - p1) * p2*(1.0 - p2))))

            rel_corr = CORREL (((double) occp->rel_ret / num_rel),
                               ((double) fdbk_info->occ[i].rel_ret / num_rel),
                               ((double) fdbk_info->occ[j].rel_ret / num_rel));
            nrel_corr = CORREL (((double) occp->nrel_ret / (num_ret - num_rel)),
                                ((double) fdbk_info->occ[i].nrel_ret / (num_ret - num_rel)),
                                ((double) fdbk_info->occ[j].nrel_ret / (num_ret -num_rel)));
            nret_corr = CORREL (((double) occp->nret / (num_docs - num_ret)),
                                ((double) fdbk_info->occ[i].nret / (num_docs - num_ret)),
                                ((double) fdbk_info->occ[j].nret / (num_docs - num_ret)));
            nrelall_corr = CORREL (((double) (occp->nret + occp->nrel_ret) / (num_docs - num_rel)),
                                   ((double) (fdbk_info->occ[i].nret + fdbk_info->occ[i].nrel_ret) / (num_docs - num_rel)),
                                   ((double) (fdbk_info->occ[j].nret + fdbk_info->occ[j].nrel_ret)/ (num_docs - num_rel)));
            
#ifdef TRACE
            if (global_trace && sm_trace >= 8) {
                fprintf(global_trace_fd, "%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%c%c\n",
                        fdbk_info->orig_query->id_num.id, occp->con, 
                        inv1.id_num, inv2.id_num, rel_count+nrel_count, ret1, ret2,
                        (float)(rel_count+nrel_count)/(float) MIN(ret1, ret2),
                        rel_corr, nrel_corr, nret_corr, nrelall_corr,
			fdbk_info->occ[i].query ? 'Q':'E',
			fdbk_info->occ[j].query ? 'Q':'E');
                (void) fflush (global_trace_fd);
            }
#endif /* TRACE */

	    occp++;
	}
    }
    coocc_info->occ = occ;
    coocc_info->num_occ = occp - occ;
    qsort((char *) occ, coocc_info->num_occ, sizeof(OCC),
	  compare_occ);

    return(1);
}

static int
merge_stats(coocc_info, fdbk_info)
FEEDBACK_INFO *coocc_info;
FEEDBACK_INFO *fdbk_info;
{
    if (fdbk_info->max_occ < fdbk_info->num_occ + coocc_info->num_occ) {
	fdbk_info->max_occ = fdbk_info->num_occ + coocc_info->num_occ;
        if (NULL == (fdbk_info->occ = 
		     Realloc(fdbk_info->occ, fdbk_info->max_occ, OCC)))
            return (UNDEF);
    }

    Bcopy(coocc_info->occ, fdbk_info->occ + fdbk_info->num_occ, 
	  coocc_info->num_occ * sizeof(OCC));

    fdbk_info->num_occ += coocc_info->num_occ;

    return (1);
}



static int
compare_occ(ptr1, ptr2)
OCC *ptr1;
OCC *ptr2;
{
    if (ptr1->con < ptr2->con)
	return(-1);
    if (ptr1->con > ptr2->con)
	return(1);
    return(0);
}
