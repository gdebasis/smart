#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libevaluate/ell_el_comp_over.c,v 11.0 1992/07/21 18:20:34 chrisb Exp chrisb $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Evaluate a set of runs, counting number of queries better than first run
 *2 ell_el_comp_over (eval_list_list, eval_list, inst)
 *3   EVAL_LIST_LIST *eval_list_list;
 *3   EVAL_LIST *eval_list;
 *3   int inst;
 *4 init_ell_el_comp_over (spec, unused)
 *5   "eval.trace"
 *4 close_ell_el_comp_over (inst)
 *7 Take an already evaluated set of runs, with evaluations for each query
 *7 in each run, and produce an EVAL_LIST object, one EVAL for each run,
 *7 the first run giving the average and all other runs giving the number
 *7 of queries which had values better than the corresponding query of
 *7 the first run.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "io.h"
#include "spec.h"
#include "trace.h"
#include "eval.h"
#include "inst.h"
#include "smart_error.h"

void print_trec_eval();

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("eval.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    long num_flt_buf;
    float *flt_buf;            /* num_buf buffers, one per query, in which 
                                   to put variable length array float
                                   values of eval */
    long num_long_buf;
    long *long_buf;            /* num_buf buffers, one per query, in which 
                                   to put variable length array long
                                   values of eval */

    EVAL *eval_buf;
    long num_eval_buf;
    char *desc_buf;
    long num_desc_buf;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static int init_results();

#define COMP(x)  if (query_eval->x > first_eval->x) result_eval->x += 1

static char *convert_desc = "   Number of queries with evaluation values over the first run";

int
init_ell_el_comp_over (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_ell_el_comp_over");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    ip = &info[new_inst];

    ip->num_eval_buf = 0;
    ip->num_desc_buf = 0;
    ip->num_flt_buf = 0;
    ip->num_long_buf = 0;

    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_ell_el_comp_over");

    return (new_inst);
}

int
ell_el_comp_over (eval_list_list, eval_list, inst)
EVAL_LIST_LIST *eval_list_list;
EVAL_LIST *eval_list;
int inst;
{
    STATIC_INFO *ip;
    long i;
    long num_run, num_query;
    EVAL_LIST *q_eval_list;
    EVAL *avg_eval, *query_eval, *first_eval, *result_eval;

    PRINT_TRACE (2, print_string, "Trace: entering ell_el_comp_over");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "ell_el_comp_over");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Reserve space and initialize to 0 the results */
    if (UNDEF == init_results (ip, eval_list_list, eval_list))
        return (UNDEF);
    /* Initialize the description field */
    if (3 + strlen (eval_list_list->description) + strlen (convert_desc) >
              ip->num_desc_buf) {
        if (0 != ip->num_desc_buf)
            (void) free (ip->desc_buf);
        ip->num_desc_buf = 2 * (3 + strlen (eval_list_list->description) +
                                strlen (convert_desc));
        if (NULL == (ip->desc_buf = malloc ((unsigned) ip->num_desc_buf)))
            return (UNDEF);
    }
    sprintf (ip->desc_buf, "%s\n%s\n\n",
             eval_list_list->description, convert_desc);

    /* Compute the averages for the first run */
    q_eval_list = &eval_list_list->eval_list[0];
    avg_eval = &eval_list->eval[0];
    /* Now want to average q_eval_list and put into avg_eval */
    for (num_query = 0; num_query < q_eval_list->num_eval; num_query++) {
        query_eval = &q_eval_list->eval[num_query];
        
        /* Add results to previous results */
        avg_eval->qid++;
        avg_eval->num_rel       += query_eval->num_rel;
        avg_eval->num_ret       += query_eval->num_ret;
        avg_eval->num_rel_ret   += query_eval->num_rel_ret;
        avg_eval->exact_recall  += query_eval->exact_recall;
        avg_eval->exact_precis  += query_eval->exact_precis;
        avg_eval->exact_rel_precis += query_eval->exact_rel_precis;
        avg_eval->av_recall_precis += query_eval->av_recall_precis;
        avg_eval->int_av_recall_precis += query_eval->int_av_recall_precis;
        avg_eval->int_nrp_av_recall_precis += query_eval->int_nrp_av_recall_precis;
        avg_eval->av_fall_recall += query_eval->av_fall_recall;
        avg_eval->R_recall_precis += query_eval->R_recall_precis;
        avg_eval->av_R_precis += query_eval->av_R_precis;
        avg_eval->int_R_recall_precis += query_eval->int_R_recall_precis;
        avg_eval->int_av_R_precis += query_eval->int_av_R_precis;
        for (i = 0; i < avg_eval->num_cutoff; i++) {
            avg_eval->recall_cut[i] += query_eval->recall_cut[i];
            avg_eval->precis_cut[i] += query_eval->precis_cut[i];
            avg_eval->rel_precis_cut[i] += query_eval->rel_precis_cut[i];
        }
        for (i = 0; i < avg_eval->num_rp_pts; i++)
            avg_eval->int_recall_precis[i] += query_eval->int_recall_precis[i];
        for (i = 0; i < avg_eval->num_fr_pts; i++)
            avg_eval->fall_recall[i] += query_eval->fall_recall[i];
        for (i = 0; i < avg_eval->num_prec_pts; i++) {
            avg_eval->R_prec_cut[i] += query_eval->R_prec_cut[i];
            avg_eval->int_R_prec_cut[i] += query_eval->int_R_prec_cut[i];
        }
        
        PRINT_TRACE (8, print_eval, avg_eval);
    }
        
    /* Calculate averages (for those eval fields returning averages) */
    if (avg_eval->qid > 0) {
        avg_eval->exact_recall /= (float) avg_eval->qid;
        avg_eval->exact_precis /= (float) avg_eval->qid;
        avg_eval->exact_rel_precis /= (float) avg_eval->qid;
        avg_eval->av_recall_precis /= (float) avg_eval->qid;
        avg_eval->int_av_recall_precis /= (float) avg_eval->qid;
        avg_eval->int_nrp_av_recall_precis /= (float) avg_eval->qid;
        avg_eval->av_fall_recall /= (float) avg_eval->qid;
        avg_eval->R_recall_precis /= (float) avg_eval->qid;
        avg_eval->av_R_precis /= (float) avg_eval->qid;
        avg_eval->int_R_recall_precis /= (float) avg_eval->qid;
        avg_eval->int_av_R_precis /= (float) avg_eval->qid;
        for (i = 0; i < avg_eval->num_cutoff; i++) {
            avg_eval->recall_cut[i] /= (float) avg_eval->qid;
            avg_eval->precis_cut[i] /= (float) avg_eval->qid;
            avg_eval->rel_precis_cut[i] /= (float) avg_eval->qid;
        }
        for (i = 0; i < avg_eval->num_rp_pts; i++)
            avg_eval->int_recall_precis[i] /= (float) avg_eval->qid;
        for (i = 0; i < avg_eval->num_fr_pts; i++)
            avg_eval->fall_recall[i] /= (float) avg_eval->qid;
        for (i = 0; i < avg_eval->num_prec_pts; i++) {
            avg_eval->R_prec_cut[i] /= (float) avg_eval->qid;
            avg_eval->int_R_prec_cut[i] /= (float) avg_eval->qid;
        }
    }

    /* Compare each query of each run to corresponding query of first run */
    for (num_run = 1; num_run < eval_list_list->num_eval_list; num_run++) {
        q_eval_list = &eval_list_list->eval_list[num_run];
        result_eval = &eval_list->eval[num_run];
        /* Now want to compare each query in q_eval_list to the first run */
        /*  COMP(x) = if (query_eval->x > first_eval->x) result_eval->x += 1 */

        for (num_query = 0; num_query < q_eval_list->num_eval; num_query++) {
            first_eval = &eval_list_list->eval_list[0].eval[num_query];
            query_eval = &q_eval_list->eval[num_query];
            COMP (qid);
            COMP (num_rel);
            COMP (num_ret);
            COMP (num_rel_ret);
            COMP (num_rel);
            COMP (exact_recall);
            COMP (exact_precis);
            COMP (exact_rel_precis);
            COMP (av_recall_precis);
            COMP (int_av_recall_precis);
            COMP (int_nrp_av_recall_precis);
            COMP (av_fall_recall);
            COMP (R_recall_precis);
            COMP (av_R_precis);
            COMP (int_R_recall_precis);
            COMP (int_av_R_precis);
            for (i = 0; i < result_eval->num_cutoff; i++) {
                COMP (recall_cut[i]);
                COMP (precis_cut[i]);
                COMP (rel_precis_cut[i]);
            }
            for (i = 0; i < result_eval->num_rp_pts; i++)
                COMP (int_recall_precis[i]);
            for (i = 0; i < result_eval->num_fr_pts; i++)
                COMP (fall_recall[i]);
            for (i = 0; i < result_eval->num_prec_pts; i++) {
                COMP (R_prec_cut[i]);
                COMP (int_R_prec_cut[i]);
            }
        }
    }
            

    /* Store run_names in description field. */
    for (num_run = 0; num_run < eval_list_list->num_eval_list; num_run++) {
        q_eval_list = &eval_list_list->eval_list[num_run];
        /* Append description to ip->desc_buf */
        if (q_eval_list->description != NULL) {
            if (3 + strlen (ip->desc_buf) + strlen (q_eval_list->description) >
                ip->num_desc_buf) {
                ip->num_desc_buf += 3 + strlen (ip->desc_buf) +
                    strlen (q_eval_list->description);
                if (NULL == (ip->desc_buf =
                             realloc (ip->desc_buf, (unsigned) ip->num_desc_buf)))
                    return (UNDEF);
            }
            sprintf (&ip->desc_buf[strlen (ip->desc_buf)],
                     "%3ld. %s\n", num_run, q_eval_list->description);
        }
    }

    eval_list->description = ip->desc_buf;

    PRINT_TRACE (4, print_eval_list, eval_list);
    PRINT_TRACE (2, print_string, "Trace: leaving ell_el_comp_over");
    return (1);
}


int
close_ell_el_comp_over (inst)
int inst;
{
    STATIC_INFO *ip;
    PRINT_TRACE (2, print_string, "Trace: entering close_ell_el_comp_over");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "ell_el_comp_over");
        return (UNDEF);
    }
    ip  = &info[inst];

    ip->valid_info--;

    if (ip->num_flt_buf > 0)
        (void) free ((char *) ip->flt_buf);
    if (ip->num_long_buf > 0)
        (void) free ((char *) ip->long_buf);
    if (ip->num_desc_buf > 0)
        (void) free ((char *) ip->desc_buf);
    if (ip->num_eval_buf > 0)
        (void) free ((char *) ip->eval_buf);

    PRINT_TRACE (2, print_string, "Trace: leaving close_ell_el_comp_over");
    return (0);
}

/* Reserve space and initialize to 0 the eval_buf results */
static int
init_results (ip, eval_list_list, eval_list)
STATIC_INFO *ip;
EVAL_LIST_LIST *eval_list_list;
EVAL_LIST *eval_list;
{
    long i,j;
    EVAL *eval, *sample_eval;
    long *long_ptr;
    float *flt_ptr;
    long num_flt_needed;

    eval_list->num_eval = eval_list_list->num_eval_list;
    if (eval_list_list->num_eval_list == 0)
        return (0);

    /* Reserve an eval_buf for each query result */
    if (eval_list_list->num_eval_list > ip->num_eval_buf) {
        if (ip->num_eval_buf > 0)
            (void) free ((char *) ip->eval_buf);
        ip->num_eval_buf = eval_list_list->num_eval_list;
        if (NULL == (ip->eval_buf = (EVAL *)
                     malloc ((unsigned) ip->num_eval_buf * sizeof (EVAL))))
            return (UNDEF);
    }
    eval_list->eval = ip->eval_buf;

    sample_eval = &eval_list_list->eval_list[0].eval[0];

    /* Reserve space for each run's float and long eval values */
    /* Assume each incoming results has the same form */
    num_flt_needed = eval_list->num_eval * (3 * sample_eval->num_cutoff +
                                            sample_eval->num_rp_pts +
                                            sample_eval->num_fr_pts +
                                            2 * sample_eval->num_prec_pts);
        if (num_flt_needed > ip->num_flt_buf) {
        ip->num_flt_buf = num_flt_needed;
        if (0 != ip->num_flt_buf)
            (void) free ((char *) ip->flt_buf);
        if (NULL == (ip->flt_buf = (float *)
                     malloc ((unsigned) ip->num_flt_buf * sizeof (float))))
            return (UNDEF);
    }
    if (eval_list->num_eval * sample_eval->num_cutoff > ip->num_long_buf) {
        ip->num_long_buf = eval_list->num_eval * sample_eval->num_cutoff;
        if (0 != ip->num_long_buf)
            (void) free ((char *) ip->long_buf);
        if (NULL == (ip->long_buf = (long *)
                     malloc ((unsigned) ip->num_long_buf * sizeof (long))))
            return (UNDEF);
    }
    (void) bzero (ip->flt_buf, ip->num_flt_buf * sizeof (float));
    (void) bzero (ip->long_buf, ip->num_long_buf * sizeof (long));
    (void) bzero (ip->eval_buf, ip->num_eval_buf * sizeof (EVAL));

    flt_ptr = ip->flt_buf;
    long_ptr = ip->long_buf;

    for (i = 0; i < eval_list->num_eval; i++) {
        eval = &eval_list->eval[i];
        eval->num_cutoff = sample_eval->num_cutoff;
        eval->num_rp_pts = sample_eval->num_rp_pts;
        eval->num_fr_pts = sample_eval->num_fr_pts;
        eval->num_prec_pts = sample_eval->num_prec_pts;
        eval->max_fall_ret = sample_eval->max_fall_ret;

        eval->cutoff = long_ptr; long_ptr += eval->num_cutoff;
        for (j = 0; j < eval->num_cutoff; j++)
            eval->cutoff[j] = sample_eval->cutoff[j];

        eval->recall_cut = flt_ptr; flt_ptr += eval->num_cutoff;
        eval->precis_cut = flt_ptr; flt_ptr += eval->num_cutoff;
        eval->rel_precis_cut = flt_ptr; flt_ptr += eval->num_cutoff;
        eval->int_recall_precis = flt_ptr; flt_ptr+= eval->num_rp_pts;
        eval->fall_recall = flt_ptr; flt_ptr+= eval->num_fr_pts;
        eval->R_prec_cut = flt_ptr; flt_ptr+= eval->num_prec_pts;
        eval->int_R_prec_cut = flt_ptr;  flt_ptr+= eval->num_prec_pts;
    }

    return (1);
}

