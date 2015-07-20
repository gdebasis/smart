#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libevaluate/rrvec_trec_eval.c,v 11.0 1992/07/21 18:20:34 chrisb Exp chrisb $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Given ranks for relevant retrieved docs for a query, evaluate the query
 *2 q_eval (eval_input, eval, inst)
 *3   EVAL_INPUT *eval_input;
 *3   EVAL *eval;
 *3   int inst;
 *4 init_q_eval (spec, unused)
 *5   "convert.eval.num_wanted"
 *5   "convert.eval.trace"
 *4 close_q_eval (inst)
 *7 Calculate various evaluation measures for this query.
 *9 WARNING: for historical reasons (eval used to be of fixed size),
 *9 q_eval cannot re-use the memory it allocates for subsidiary structures
 *9 of "eval".  Calling procedures can count on the space for one query's
 *9 evaluation remaining valid evan after the next query is evaluated.
 *9 (This is counter to standard SMART memory practices, but it's too much 
 *9 work to rewrite it efficiently)
***********************************************************************/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "io.h"
#include "rel_header.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "eval.h"
#include "inst.h"

static char *cutoff_list;
static long num_rp_pts;
static long num_fr_pts;
static long max_fall_ret;
static long num_prec_pts;

static SPEC_PARAM spec_args[] = {
    {"eval.cutoff_list",     getspec_string, (char *) &cutoff_list},
    {"eval.num_rp_pts",     getspec_long, (char *) &num_rp_pts},
    {"eval.num_fr_pts",     getspec_long, (char *) &num_fr_pts},
    {"eval.max_fall_ret",     getspec_long, (char *) &max_fall_ret},
    {"eval.num_prec_pts",     getspec_long, (char *) &num_prec_pts},
    TRACE_PARAM ("eval.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);


/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    EVAL initial_eval;          /* The known initial values from spec file */

    long num_buf;
    long max_num_buf;
    float **flt_buf;            /* num_buf buffers, one per query, in which 
                                   to put variable length array float
                                   values of eval */
    long **long_buf;            /* num_buf buffers, one per query, in which 
                                   to put variable length array long
                                   values of eval */
    long size_flt_buf;          /* Size in bytes of each flt_buf to be
                                   allocated for a query */

    long *temp_ptr;             /* space reserved for each query's cutoff
                                   values */
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

#define INIT_NUM_QUERY 500
void print_eval();

static int init_eval(), init_initial_eval();

int
init_q_eval (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;
    
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_q_eval");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    ip = &info[new_inst];
    
    if (UNDEF == init_initial_eval (&ip->initial_eval))
        return (UNDEF);
    ip->size_flt_buf = (3 * ip->initial_eval.num_cutoff +
                        ip->initial_eval.num_rp_pts +
                        ip->initial_eval.num_fr_pts + 
                        2 * ip->initial_eval.num_prec_pts) * sizeof (float);

    ip->num_buf = 0;
    ip->max_num_buf = INIT_NUM_QUERY;
    if (NULL == (ip->flt_buf = (float **)
                 malloc ((unsigned) INIT_NUM_QUERY * sizeof (float *))) ||
        NULL == (ip->long_buf = (long **)
                 malloc ((unsigned) INIT_NUM_QUERY * sizeof (long *))))
        return (UNDEF);

    /* Reserve space for number of docs needed to have seen for each of the
       various cutoff values */
    if (NULL == (ip->temp_ptr = (long *)
                 malloc ((unsigned) (ip->initial_eval.num_rp_pts +
                                     ip->initial_eval.num_fr_pts +
                                     ip->initial_eval.num_prec_pts)
                         * sizeof (long))))
        return (UNDEF);


    ip->valid_info = 1;
    
    PRINT_TRACE (2, print_string, "Trace: leaving init_q_eval");
    return (new_inst);
}


int
q_eval (eval_input, eval, inst)
EVAL_INPUT *eval_input;
EVAL *eval;
int inst;
{
    double recall, precis, int_precis;  /* current recall, precision and
                                           interpolated precision values */
    long i,j;
    long rel_so_far;
    long *cut_rp;             /* number of rel docs needed to be retrieved
                                 for each recall-prec cutoff */
    long *cut_fr;             /* number of non-rel docs needed to be
                                 retrieved for each fall-recall cutoff */
    long *cut_rprec;          /* Number of docs needed to be retrieved
                                 for each R-based prec cutoff */
    long current_cutoff, current_cut_rp, current_cut_fr, current_cut_rprec;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering q_eval");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "q_eval");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (UNDEF == init_eval (ip, eval, eval_input))
        return (UNDEF);

    /* If no relevant docs, then just return */
    if (eval->num_rel == 0) {
        PRINT_TRACE (2, print_string, "Trace: leaving q_eval");
        return (0);
    }

    cut_rp = ip->temp_ptr;
    cut_fr = &ip->temp_ptr[eval->num_rp_pts];
    cut_rprec = &cut_fr[eval->num_fr_pts];

    /* Discover cutoff values for this query */
    current_cutoff = eval->num_cutoff - 1;
    while (current_cutoff > 0 && eval->cutoff[current_cutoff] > eval->num_ret)
        current_cutoff--;
    for (i = 0; i < eval->num_rp_pts; i++)
        cut_rp[i] = ((eval->num_rel * i) + eval->num_rp_pts - 2) / (eval->num_rp_pts - 1);
    current_cut_rp = eval->num_rp_pts - 1;
    while (current_cut_rp > 0 && cut_rp[current_cut_rp] > eval->num_rel_ret)
        current_cut_rp--;
    for (i = 0; i < eval->num_fr_pts; i++)
        cut_fr[i] = ((eval->max_fall_ret * i) + eval->num_fr_pts - 2) / (eval->num_fr_pts - 1);
    current_cut_fr = eval->num_fr_pts - 1;
    while (current_cut_fr > 0 && cut_fr[current_cut_fr] > eval->num_ret - eval->num_rel_ret)
        current_cut_fr--;
    for (i = 0; i < eval->num_prec_pts; i++)
        cut_rprec[i] = ((MAX_RPREC * eval->num_rel * i) + eval->num_prec_pts - 2) 
                        / (eval->num_prec_pts - 1);
    current_cut_rprec = eval->num_prec_pts - 1;
    while (current_cut_rprec > 0 &&
           cut_rprec[current_cut_rprec] > eval->num_ret)
        current_cut_rprec--;

    /* For now, somewhat inefficiently use the same loop as in trvec_teval. */
    rel_so_far = eval->num_rel_ret;
    /* Note for interpolated precision values (Prec(X) = MAX (PREC(Y)) for all
       Y >= X) */
    int_precis = (float) rel_so_far / (float) eval->num_ret;
    for (j = eval->num_ret; j > 0; j--) {
        recall = (float) rel_so_far / (float) eval->num_rel;
        precis = (float) rel_so_far / (float) j;
        if (int_precis < precis)
            int_precis = precis;
        while (current_cutoff >= 0 && j == eval->cutoff[current_cutoff]) {
            eval->recall_cut[current_cutoff] = recall;
            eval->precis_cut[current_cutoff] = precis;
            eval->rel_precis_cut[current_cutoff] = (j > eval->num_rel) ? recall
                : precis;
            current_cutoff--;
        }
        while (current_cut_rprec >= 0 && j == cut_rprec[current_cut_rprec]) {
            eval->R_prec_cut[current_cut_rprec] = precis;
            eval->int_R_prec_cut[current_cut_rprec] = int_precis;
            current_cut_rprec--;
        }

        if (j == eval->num_rel) {
            eval->R_recall_precis = precis;
            eval->int_R_recall_precis = int_precis;
        }

        if (j < eval->num_rel) {
            eval->av_R_precis += precis;
            eval->int_av_R_precis += int_precis;
        }

        if (rel_so_far > 0 && j == eval_input->rel_ret_rank[rel_so_far-1]) {
            eval->int_av_recall_precis += int_precis;
            eval->av_recall_precis += precis;
            while (current_cut_rp>=0 && rel_so_far == cut_rp[current_cut_rp]) {
                eval->int_recall_precis[current_cut_rp] = int_precis;
                current_cut_rp--;
            }
            rel_so_far--;
        }
        else {
            /* Note: for fallout-recall, the recall at X non-rel docs
               is used for the recall 'after' (X-1) non-rel docs.
               Ie. recall_used(X-1 non-rel docs) = MAX (recall(Y)) for 
               Y retrieved docs where X-1 non-rel retrieved */
            while (current_cut_fr >= 0 &&
                   j - rel_so_far == cut_fr[current_cut_fr] + 1) {
                eval->fall_recall[current_cut_fr] = recall;
                current_cut_fr--;
            }
            if (j - rel_so_far < eval->max_fall_ret) {
                eval->av_fall_recall += recall;
            }
        }
    }


    /* Fill in the 0.0 value for recall-precision (== max precision
       at any point in the retrieval ranking) */
    eval->int_recall_precis[0] = int_precis;

    /* Fill in those cutoff values and averages that were not achieved
       because insufficient docs were retrieved. */
    for (i = 0; i < eval->num_cutoff; i++) {
        if (eval->num_ret < eval->cutoff[i]) {
            eval->recall_cut[i] = ((float) eval->num_rel_ret /
                                   (float) eval->num_rel);
            eval->precis_cut[i] = ((float) eval->num_rel_ret / 
                                   (float) eval->cutoff[i]);
            eval->rel_precis_cut[i] = (eval->cutoff[i] < eval->num_rel) ?
                                            eval->precis_cut[i] :
                                            eval->recall_cut[i];
        }
    }
    for (i = 0; i < eval->num_fr_pts; i++) {
        if (eval->num_ret - eval->num_rel_ret < cut_fr[i]) {
            eval->fall_recall[i] = (float) eval->num_rel_ret / 
                                   (float) eval->num_rel;
        }
    }
    if (eval->num_ret - eval->num_rel_ret < eval->max_fall_ret) {
        eval->av_fall_recall += ((eval->max_fall_ret - 
                                  (eval->num_ret - eval->num_rel_ret))
                                 * ((float)eval->num_rel_ret / 
                                    (float)eval->num_rel));
    }
    if (eval->num_rel > eval->num_ret) {
        eval->R_recall_precis = (float) eval->num_rel_ret / 
                                (float)eval->num_rel;
        eval->int_R_recall_precis = (float) eval->num_rel_ret / 
                                    (float)eval->num_rel;
        for (i = eval->num_ret; i < eval->num_rel; i++) {
            eval->av_R_precis += (float) eval->num_rel_ret / (float) i;
            eval->int_av_R_precis += (float) eval->num_rel_ret / (float) i;
        }
    }
    for (i = 0; i < eval->num_prec_pts; i++) {
        if (eval->num_ret < cut_rprec[i]) {
            eval->R_prec_cut[i] = (float) eval->num_rel_ret / 
                (float) cut_rprec[i];
            eval->int_R_prec_cut[i] = (float) eval->num_rel_ret / 
                (float) cut_rprec[i];
        }
    }

    /* The following cutoffs/averages are correct, since 0.0 should
       be averaged in for the non-retrieved docs:
       av_recall_precis, int_av_recall_prec, int_recall_prec,
       int_av_recall_precis
    */


    /* Calculate other indirect evaluation measure averages. */
    for (i = 0; i < eval->num_rp_pts; i++) {
        eval->int_nrp_av_recall_precis += eval->int_recall_precis[i];
    }
    eval->int_nrp_av_recall_precis /= eval->num_rp_pts;


    /* Calculate all the other averages */
    if (eval->num_rel_ret > 0) {
        eval->av_recall_precis /= eval->num_rel;
        eval->int_av_recall_precis /= eval->num_rel;
    }

    eval->av_fall_recall /= eval->max_fall_ret;

    if (eval->num_rel) {
        eval->av_R_precis /= eval->num_rel;
        eval->int_av_R_precis /= eval->num_rel;
        eval->exact_recall = (double) eval->num_rel_ret / eval->num_rel;
        eval->exact_precis = (double) eval->num_rel_ret / eval->num_ret;
        if (eval->num_rel > eval->num_ret)
            eval->exact_rel_precis = eval->exact_precis;
        else
            eval->exact_rel_precis = eval->exact_recall;
    }

    PRINT_TRACE (4, print_eval, eval);
    PRINT_TRACE (2, print_string, "Trace: leaving q_eval");
    return (1);
}


int
close_q_eval (inst)
int inst;
{
    STATIC_INFO *ip;
    long i;

    PRINT_TRACE (2, print_string, "Trace: entering close_q_eval");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_q_eval");
        return (UNDEF);
    }
    ip  = &info[inst];

    ip->valid_info--;

    for (i = 0; i < ip->num_buf; i++) {
        (void) free ((char *) ip->flt_buf[i]);
        (void) free ((char *) ip->long_buf[i]);
    }
    (void) free ((char *) ip->long_buf);
    (void) free ((char *) ip->flt_buf);
    (void) free ((char *) ip->initial_eval.cutoff);
    (void) free ((char *) ip->temp_ptr);

    PRINT_TRACE (2, print_string, "Trace: leaving close_q_eval");
    return (0);
}

static int
init_initial_eval (eval)
EVAL *eval;
{
    char *ptr, *buf_ptr;
    char *buf;
    long i;

    (void) bzero ((char *) eval, sizeof (EVAL));
    eval->num_rp_pts = num_rp_pts;
    eval->num_fr_pts = num_fr_pts;
    eval->max_fall_ret = max_fall_ret;
    eval->num_prec_pts = num_prec_pts;

    if (NULL == (buf = malloc ((unsigned) strlen(cutoff_list) + 1)))
        return (UNDEF);

    eval->num_cutoff = 0;
    ptr = cutoff_list;
    while (*ptr) {
        while (*ptr && isspace (*ptr))
            ptr++;
        if (*ptr) {
            while (*ptr && ! isspace (*ptr))
                ptr++;
            eval->num_cutoff++;
        }
    }

    if (NULL == (eval->cutoff = (long *)
                 malloc ((unsigned) eval->num_cutoff * sizeof (long))))
        return (UNDEF);

    i = 0;
    ptr = cutoff_list;
    while (*ptr) {
        while (*ptr && isspace (*ptr))
            ptr++;
        if (*ptr) {
            buf_ptr = buf; 
            while (*ptr && ! isspace (*ptr))
                *buf_ptr++ = *ptr++;
            *buf_ptr = '\0';
            eval->cutoff[i++] = atol (buf);
        }
    }

    (void) free (buf);

    return (0);
}

static int
init_eval (ip, eval, eval_input)
STATIC_INFO *ip;
EVAL *eval;
EVAL_INPUT *eval_input;
{
    float *ptr;

    if (ip->num_buf >= ip->max_num_buf) {
        ip->max_num_buf += ip->num_buf;
        if (NULL == (ip->flt_buf = (float **)
                 realloc (ip->flt_buf,
                          (unsigned) ip->max_num_buf * sizeof (float *))) ||
            NULL == (ip->long_buf = (long **)
                     realloc (ip->long_buf,
                              (unsigned) ip->max_num_buf * sizeof (long *))))
            return (UNDEF);
    }

    (void) bcopy ((char *) &ip->initial_eval, (char *) eval, sizeof (EVAL));
    if (NULL == (ip->long_buf[ip->num_buf] = (long *)
                 malloc ((unsigned) eval->num_cutoff * sizeof (long))) ||
        NULL == (ip->flt_buf[ip->num_buf] = (float *)
                 malloc (ip->size_flt_buf)))
        return (UNDEF);
    eval->cutoff = ip->long_buf[ip->num_buf];
    (void) bcopy ((char *) ip->initial_eval.cutoff, (char *) eval->cutoff,
                  sizeof (long) * eval->num_cutoff);

    ptr = ip->flt_buf[ip->num_buf];
    (void) bzero ((char *) ptr, ip->size_flt_buf);
    eval->recall_cut = ptr; ptr += eval->num_cutoff;
    eval->precis_cut = ptr; ptr += eval->num_cutoff;
    eval->rel_precis_cut = ptr; ptr += eval->num_cutoff;
    eval->int_recall_precis = ptr; ptr+= eval->num_rp_pts;
    eval->fall_recall = ptr; ptr+= eval->num_fr_pts;
    eval->R_prec_cut = ptr; ptr+= eval->num_prec_pts;
    eval->int_R_prec_cut = ptr; /* ptr+= eval->num_prec_pts; */

    eval->qid = eval_input->qid;
    eval->num_rel_ret = eval_input->num_rel_ret;
    eval->num_rel = eval_input->num_rel;
    eval->num_ret = eval_input->num_ret;

    ip->num_buf++;
    return (1);
}

