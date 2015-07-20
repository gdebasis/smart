#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libretrieve/sim_trec_cty.c,v 11.2 1993/02/03 16:52:55 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Find optimal operator weights for pnorm queries
 *1 local.convert.obj.best_pqvec
 *2 best_pqvec_obj (in_file, out_file, inst)
 *3   char *in_file, *out_file;
 *3   int inst;
 *4 init_best_pqvec_obj (spec, param_prefix)
 *4 close_best_pqvec_obj (inst)
 *7 Adapted from liblocal/libfeedback/best_ret.c
 *9 WARNING: could run into trouble if the qrels_file is for a larger 
 *9 collection than the database (e.g. if the qrels_file is for d1234
 *9 and the database is d23).
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "context.h"
#include "retrieve.h"
#include "rel_header.h"
#include "vector.h"
#include "inv.h"
#include "tr_vec.h"
#include "rr_vec.h"
#include "inst.h"
#include "pnorm_vector.h"
#include "pnorm_common.h"
#include "local_functions.h"

static char *textloc_file;
static char *qrels_file;
static long qrels_mode;
static long num_wanted;
static long use_R_prec;
static long read_mode, write_mode;
static double and_max, or_max, and_incr, or_incr;
static SPEC_PARAM spec_args[] = {
    {"convert.pvec.textloc_file", getspec_dbfile, (char *) &textloc_file},
    {"convert.pvec.best_terms.qrels_file", getspec_dbfile, (char *) &qrels_file},
    {"qrels_file.rmode", getspec_filemode, (char *) &qrels_mode},
    {"convert.pvec.num_wanted", getspec_long, (char *) &num_wanted},
    {"convert.pvec.use_R_prec", getspec_bool, (char *) &use_R_prec},
    {"convert.pvec.rmode", getspec_filemode, (char *) &read_mode},
    {"convert.pvec.rwcmode", getspec_filemode, (char *) &write_mode},
    {"convert.pvec.and_max", getspec_double, (char *) &and_max},
    {"convert.pvec.or_max", getspec_double, (char *) &or_max},
    {"convert.pvec.and_incr", getspec_double, (char *) &and_incr},
    {"convert.pvec.or_incr", getspec_double, (char *) &or_incr},    
    TRACE_PARAM ("convert.pvec.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

/* Result data structures:
 * long num_wanted -- number of top docs to keep track of
 * float *full_results --  sims for best query found so far
 * TOP_RESULT *top_results -- sims for top docs of best query found so far,
 *                            sorted by docid
 * float *min_top_sim -- minimum sim for above
 * TOP_RESULT *current_new_results --  sims for docs including term under 
 * consideration which are greater than min_top_sim, sorted by docid
 * TOP_REL_RESULT *new_results --  Merge of top_results and current_results,
 * adding relevance info.
 * RR_VEC qrels;  Relevance info for this query, sorted by docid
 */

static int ret_inst;
static float *full_results;
static TOP_RESULT *top_results;
static RESULT results;
static RR_VEC qrels;
static int qrels_fd;
static float current_eval;	/* Evaluation of current best query */

typedef struct {
    long did;
    float sim;
    int rel;
} TOP_REL_RESULT;
static TOP_REL_RESULT *new_results;

static int optimize_qvec(), run_query();
static int eval(), eval_top();
static int next_setting();
static int comp_did(), comp_sim_did();

int init_sim_ctype_inv_p(), sim_ctype_inv_p(), close_sim_ctype_inv_p();


int
init_best_pqvec_obj (spec, unused)
SPEC *spec;
char *unused;
{
    REL_HEADER *rh;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_best_pqvec_obj");

    /* Reserve space for all partial sims */
    if (NULL == (rh = get_rel_header (textloc_file)) || 
        rh->max_primary_value <= 0) {
        set_error (SM_ILLPA_ERR, "Cannot open textloc file", "best_pqvec_obj");
        return (UNDEF);
    }
    results.num_full_results = rh->max_primary_value + 1;
    if (num_wanted > results.num_full_results)
	num_wanted = results.num_full_results;
    if (use_R_prec)
	results.num_top_results = results.num_full_results;
    else 
	results.num_top_results = num_wanted;

    if (NULL == (full_results = Malloc(results.num_full_results, float)) ||
        NULL == (top_results = Malloc(results.num_top_results, TOP_RESULT)) ||
        NULL == (new_results = Malloc(results.num_full_results, TOP_REL_RESULT))) {
        set_error (errno, "2", "best_pqvec_obj");
        return (UNDEF);
    }
    results.full_results = full_results;
    results.top_results = top_results;

    if (UNDEF == (qrels_fd = open_rr_vec (qrels_file, qrels_mode)))
        return (UNDEF);
    if (UNDEF == (ret_inst = init_sim_ctype_inv_p(spec, NULL)))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_best_pqvec_obj");
    return (0);
}

int
best_pqvec_obj (in_file, out_file, inst)
char *in_file, *out_file;
int inst;
{
    int in_index, out_index, status;
    PNORM_VEC vector;

    PRINT_TRACE (2, print_string, "Trace: entering best_pqvec_obj");

    if (UNDEF == (in_index = open_pnorm (in_file, read_mode)),
	UNDEF == (out_index = open_pnorm (out_file, write_mode)))
        return (UNDEF);

    vector.id_num.id = 0; EXT_NONE(vector.id_num.ext);
    if (global_start > 0) {
        vector.id_num.id = global_start;
        if (UNDEF == seek_pnorm (in_index, &vector))
            return (UNDEF);
    }
    while (1 == (status = read_pnorm (in_index, &vector)) &&
           vector.id_num.id <= global_end) {
	qrels.qid = vector.id_num.id;
	if (UNDEF == seek_rr_vec(qrels_fd, &qrels) ||
	    UNDEF == read_rr_vec(qrels_fd, &qrels))
	    return (UNDEF);
	if (qrels.qid ==  vector.id_num.id &&
	    qrels.num_rr > 0) {

printf("Doing query %ld ", vector.id_num.id);

	    if (UNDEF == optimize_qvec(vector))
		return(UNDEF);

printf("\n");

	    if (UNDEF == seek_pnorm (out_index, &vector) ||
		1 != write_pnorm (out_index, &vector))
		return (UNDEF);
	}
    }

    if (UNDEF == close_pnorm (in_index) ||
	UNDEF == close_pnorm (out_index))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving best_pqvec_obj");
    return (1);
}

int
close_best_pqvec_obj (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_best_pqvec_obj");

    if (UNDEF == close_sim_ctype_inv_p(ret_inst))
	return(UNDEF);

    Free(full_results); Free(top_results);
    Free(new_results);

    PRINT_TRACE (2, print_string, "Trace: leaving close_best_pqvec_obj");
    return (0);
}


/* Try to improve the current retrieval results by adding term given
   by occinfo, with weight given by try_weight.  If successful,
   replace occinfo->weight with try_weight, and replace results with
   the results calculated including the new weight.  If failure,
   replace occinfo->weight with fail_weight, and do nothing to results.
*/

static int
optimize_qvec(qvec)
PNORM_VEC *qvec;
{
    int numop, i;
    float curr_res, best_res = -1.0;
    PNORM_VEC tmp_vec;

    numop = qvec->num_op_p_wt;
    if (0 == numop)
	return 0;

    tmp_vec = *qvec;
    if (NULL == (tmp_vec.op_p_wtp = Malloc(numop, OP_P_WT)))
	return(UNDEF);
    for (i = 0; i < numop; i++) {
	tmp_vec.op_p_wtp[i] = qvec->op_p_wtp[i];
	tmp_vec.op_p_wtp[i].p = 1.0;
	/* Initialize tmp_vec.op_p_wtp */
    }

    do {
	if (UNDEF == run_query(&tmp_vec, &curr_res))
	    return(UNDEF);
	if (curr_res > best_res) {
	    best_res = curr_res;
	    for (i = 0; i < numop; i++) 
		qvec->op_p_wtp[i].p = tmp_vec.op_p_wtp[i].p;
	    /* save results as well */
	}
    } while (next_setting(&tmp_vec));

    Free(tmp_vec.op_p_wtp);
    return 1;
}

static int
run_query(qvec, eval_result)
PNORM_VEC *qvec;
float *eval_result;
{
    int i;

#if 0
printf("."); fflush(stdout);
#endif

    if (use_R_prec)
        results.num_top_results = qrels.num_rr;

    /* Reset everything for this query */
    /* Ensure all values in full_results and top_results are 0 initially */
    bzero((char *) full_results, 
	  sizeof (float) * (int) (results.num_full_results));
    bzero((char *) top_results, 
	  sizeof (TOP_RESULT) * (int) (results.num_top_results));
    results.min_top_sim = 0.0;
    current_eval = 0.0;

    /* Run retrieval */
    if (UNDEF == sim_ctype_inv_p(qvec, &results, ret_inst))
	return(UNDEF);
    /* Re-establish full_results value (ctype proc can realloc if needed) */
    full_results = results.full_results;

    for (i = 0; i < results.num_top_results; i++) {
        if (top_results[i].did == 0)
            break;
    }
    results.num_top_results = i;
    qsort((char *) top_results, results.num_top_results, sizeof(TOP_RESULT), 
	  comp_did);

    /* Evaluate run */
    (void) eval_top (eval_result);

#if 1
print_pnorm_vector(qvec, NULL);
printf("%4.4f\n\n", *eval_result);
#endif

    return 1;
}

static int
eval (new_ptr,  new_results, new_eval)
TOP_REL_RESULT *new_ptr;
TOP_REL_RESULT *new_results;
float *new_eval;
{
    long num_rel, num_nonrel;
    TOP_REL_RESULT *end_new_ptr;

    /* Evaluate results. If feedback.use_R_prec is true, then evaluate by
       precision at qrels.num_rr.  Otherwise, evaluate by average precision
       over the top feedback.num_wanted docs. */
    if (use_R_prec) {
        if (new_ptr - new_results > qrels.num_rr)
            end_new_ptr = &new_results[qrels.num_rr];
        else
            end_new_ptr = new_ptr;
        new_ptr = new_results;
        num_rel = 0;
        while (new_ptr < end_new_ptr)
            if (new_ptr->rel)
                num_rel++;
        *new_eval = (double) num_rel / qrels.num_rr;
    }
    else {
        *new_eval = 0.0;
        if (new_ptr - new_results > num_wanted)
            end_new_ptr = &new_results[num_wanted];
        else
            end_new_ptr = new_ptr;
        new_ptr = new_results;
        num_rel = 0; num_nonrel = 0;
        while (new_ptr < end_new_ptr) {
PRINT_TRACE (20, print_long, &new_ptr->did);
PRINT_TRACE (20, print_long, &new_ptr->rel);
PRINT_TRACE (20, print_float, &new_ptr->sim);
            if (new_ptr->rel) {
                num_rel++;
                *new_eval += (double) num_rel / (num_rel + num_nonrel);
#ifdef TRACE
    if (global_trace && trace >= 12) {
        fprintf (global_trace_fd, "New_eval incr. Now %6.4f\n", *new_eval);
    }
#endif  /* TRACE */
            }
            else
                num_nonrel++;
            new_ptr++;
        }
        *new_eval = *new_eval / qrels.num_rr;
#ifdef TRACE
    if (global_trace && trace >= 12) {
        fprintf (global_trace_fd, "New_eval final. Now %6.4f\n", *new_eval);
    }
#endif  /* TRACE */
    }        
    return (1);
}

static int
eval_top (new_eval)
float *new_eval;
{
    TOP_REL_RESULT *new_ptr;
    TOP_RESULT *top_ptr, *end_top_ptr;
    RR_TUP *qrels_ptr, *end_qrels_ptr;

    top_ptr = top_results;
    end_top_ptr = &top_results[results.num_top_results];
    qrels_ptr = qrels.rr;
    end_qrels_ptr = &qrels.rr[qrels.num_rr];
    new_ptr = new_results;

    while (top_ptr < end_top_ptr) {
        new_ptr->did = top_ptr->did;
        new_ptr->sim = top_ptr->sim;
        top_ptr++;
        while (qrels_ptr < end_qrels_ptr && qrels_ptr->did < new_ptr->did)
            qrels_ptr++;
        if (qrels_ptr < end_qrels_ptr && qrels_ptr->did == new_ptr->did)
            new_ptr->rel = 1;
        else
            new_ptr->rel = 0;
        new_ptr++;
    }
    /* Sort new_results by sim and then docid */
    qsort ((char *) new_results, new_ptr-new_results,
           sizeof (TOP_REL_RESULT), comp_sim_did);
    (void) eval (new_ptr, new_results, new_eval);
    return (1);
}


static int
next_setting(qvec)
PNORM_VEC *qvec;
{
    int carry, i;
    double max, incr;

    for (carry = 1, i = 0; i < qvec->num_op_p_wt; i++) {
	if (AND_OP == qvec->op_p_wtp[i].op) {
	    max = and_max;
	    incr = and_incr;
	}
	else if (OR_OP == qvec->op_p_wtp[i].op) {
	    max = or_max;
	    incr = or_incr;
	}
	else continue;
	if (qvec->op_p_wtp[i].p == max) {
	    qvec->op_p_wtp[i].p = 1.0;
	}
	else { 
	    qvec->op_p_wtp[i].p += incr;
	    carry = 0;
	    break;
	}
    }
    if (carry)
	return 0;
    else return 1;
}


static int
comp_sim_did (ptr1, ptr2)
TOP_REL_RESULT *ptr1;
TOP_REL_RESULT *ptr2;
{
    if (ptr1->sim > ptr2->sim ||
        (ptr1->sim == ptr2->sim &&
         ptr1->did > ptr2->did))
        return (-1);
    return (1);
}


static int
comp_did (ptr1, ptr2)
TOP_RESULT *ptr1;
TOP_RESULT *ptr2;
{
    return (ptr1->did - ptr2->did);
}
