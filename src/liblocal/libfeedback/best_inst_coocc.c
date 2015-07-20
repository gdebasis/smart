#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libretrieve/sim_trec_cty.c,v 11.2 1993/02/03 16:52:55 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Perform inverted retrieval algorithm for a single ctype query 
 *1 local.feedback.best_terms.ret_inst
 *2 best_ret_inst_coocc (num_to_add, fdbk_info, inst)
 *3   long *num_to_add;
 *3   FEEDBACK_INFO *fdbk_info;
 *3   int inst;
 *4 init_best_ret_inst_coocc (spec, param_prefix)
 *5   "feedback.best_terms.trace"
 *5   "ctype.*.inv_file"
 *5   "ctype.*.inv_file.rmode"
 *5   "ctype.*.sim_ctype_weight"
 *4 close_best_ret_inst_coocc (inst)

 *7 This uses the work-horse retrieval procedure.  It takes query
 *7 terms as specified in fdbk_info, most often a single ctype vector, 
 *7 and dynamically runs subsets of the query terms to decide on a near
 *7 optimum and runs them against the inverted file specified by 
 *7 the parameter whose name is the concatenation of param_prefix
 *7 and "inv_file".  Normally this parameter name will be
 *7 "ctype.N.inv_file", where N is the ctype for this query.
 *7 The results are accumulated in the data structures given by results,
 *7 which may contain partial results from other ctypes of this query vector.
 *7 The basic algorithm is
 *7    for each concept C in qvec
 *7        for each document D in inverted list for concept C
 *7            full_results (D) = full_results (D) + 
 *7               sim_ctype_weight * (weight of C in qvec) * (weight of C in D)
 *7 The array results->full_results contains the partial similarity 
 *7 of document D to the full query so far.
 *7 The array results->top_results contains the top num_wanted similarities 
 *7 so far computed.  These are the docs that will be returned to the user.
 *7 This array is maintained by procedure rank_tr.
 *8 best_ret_inst_coocc is most often called by feedback.form_query.form_best()
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
#include "feedback.h"

/*  Perform inner product similarity function using inverted file to access */
/*  document weights.  Query weights given by query vector.  Weight for */
/*  type of query term (ctype weight) is given by ret_spec file. */

/* Basic algorithm: malloc enough space to keep track of all similarities */
/*  of documents with direct access by did. */
/*  Top num_wanted similarites kept track of throughout. */

static char *textloc_file;
static long num_best_ratio;
static long num_ctypes;
static long num_wanted;
static char *qrels_file;
static long qrels_mode;
static PROC_TAB *tokcon;

static SPEC_PARAM spec_args[] = {
    {"feedback.textloc_file",   getspec_dbfile,  (char *) &textloc_file},
    {"feedback.best_terms.qrels_file", getspec_dbfile,  (char *) &qrels_file},
    {"qrels_file.rmode",        getspec_filemode,(char *) &qrels_mode},
    {"feedback.num_best_ratio", getspec_long,    (char *) &num_best_ratio},
    {"feedback.num_ctypes",     getspec_long,    (char *) &num_ctypes},
    {"feedback.num_wanted",     getspec_long,    (char *) &num_wanted},
    {"feedback.coocc.ctype.0.token_to_con", getspec_func, (char *) &tokcon},
    TRACE_PARAM ("feedback.best_terms.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static char *prefix;
static char *inv_file;
static long inv_mode;
static float ctype_weight;
static SPEC_PARAM_PREFIX pspec_args[] = {
    {&prefix,  "inv_file",     getspec_dbfile,    (char *) &inv_file},
    {&prefix,  "inv_file.rmode", getspec_filemode,(char *) &inv_mode},
    {&prefix,  "sim_ctype_weight", getspec_float, (char *) &ctype_weight},
    };
static int num_pspec_args = sizeof (pspec_args) / sizeof (pspec_args[0]);

static float current_best_ratio;
static SPEC_PARAM_PREFIX wspec_args[] = {
    {&prefix,  "best_ratio",     getspec_float, (char *) &current_best_ratio},
    };
static int num_wspec_args = sizeof (wspec_args) / sizeof (wspec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    /*inverted file info */
    char file_name[PATH_LEN];
    int inv_fd;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;


/* Result data structures: */
/* long num_wanted; number of top docs to keep track of */
/* float *full_results;  sims for best query found so far */
/* TOP_RESULT *top_results; sims for top docs of best query found so far,
   sorted by docid */
/* float *min_top_sim;  minimum sim for above */
/* TOP_RESULT *current_new_results;  sims for docs including term under 
   consideration which are greater than min_top_sim, sorted by docid */
/* TOP_REL_RESULT *new_results;  Merge of top_results and current_results,
   adding relevance info. */
/* RR_VEC qrels;  Relevance info for this query, sorted by docid */

static float *full_results;

static TOP_RESULT *top_results;

static TOP_RESULT *current_new_results;
static long num_current_new_results;

typedef struct {
    long did;
    float sim;
    int rel;
} TOP_REL_RESULT;

static TOP_REL_RESULT *new_results;

static RR_VEC qrels;       /* Relevance info for current query. */

static float current_eval;    /* Evaluation of best query variation to date */

static int *inv_fd;
static float *best_ratio;

static int qrels_fd;

static RESULT results;

static int contok_inst, tokcon_inst;

static int comp_did(), comp_sim_did(), compare_occ_info(),
try_improve(), do_orig();
static int eval(), eval_top();
static int get_coocc_inv();
int init_contok_dict_noinfo(), contok_dict_noinfo(), close_contok_dict_noinfo();

static LISTWT *lwp;
static long num_lw, max_num_lw;

int
init_best_ret_inst_coocc (spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    STATIC_INFO *ip;
    int new_inst;
    char param_buf[100];
    REL_HEADER *rh;
    long i;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_best_ret_inst_coocc");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];

    /* Reserve space for all partial sims */
    if (NULL == (rh = get_rel_header (textloc_file)) || 
        rh->max_primary_value <= 0) {
        set_error (SM_ILLPA_ERR, "Cannot open textloc file", "best_ret_inst_coocc");
        return (UNDEF);
    }
    results.num_full_results = rh->max_primary_value + 1;

    num_current_new_results = 10*num_wanted;
    if (NULL == (full_results = (float *) 
                 malloc ((unsigned) (results.num_full_results) *
			 sizeof (float))) ||
	/* +1 before I am in a hurry and don't want to figure usage in rank_did */
        NULL == (top_results = (TOP_RESULT *) 
                 malloc ((unsigned)  MIN(num_wanted+1, results.num_full_results) *
			 sizeof (TOP_RESULT))) ||
        NULL == (current_new_results = (TOP_RESULT *) 
                 malloc ((unsigned) (num_current_new_results) *
			 sizeof (TOP_RESULT))) ||
        /* this can have all current_new_results + all top_results */
        NULL == (new_results = (TOP_REL_RESULT *) 
                 malloc ((unsigned) (num_current_new_results+num_wanted) *
			 sizeof (TOP_REL_RESULT)))) {
        set_error (errno, "1", "best_ret_inst_coocc");
        return (UNDEF);
    }

    results.full_results = full_results;
    results.top_results = top_results;
    results.num_top_results = MIN(num_wanted, results.num_full_results);

    if (NULL == (inv_fd = (int *) 
                 malloc ((unsigned) num_ctypes * sizeof (int))) ||
        NULL == (best_ratio = (float *)
                 malloc ((unsigned) num_best_ratio * sizeof (float)))) {
        set_error (errno, "2", "best_ret_inst_coocc");
        return (UNDEF);
    }
    
    if (UNDEF == (qrels_fd = open_rr_vec (qrels_file, qrels_mode)))
        return (UNDEF);

    /* Get and Open inverted file for each ctype */
    for (i = 0; i < num_ctypes; i++) {
        (void) sprintf (param_buf, "feedback.best_terms.ctype.%ld.", i);
        prefix = param_buf;
        if (UNDEF == lookup_spec_prefix (spec, &pspec_args[0], num_pspec_args))
            return (UNDEF);
        if (! VALID_FILE (inv_file))
	    continue;
        PRINT_TRACE (4, print_string, inv_file);
        if (UNDEF == (inv_fd[i] = open_inv(inv_file, inv_mode)))
            return (UNDEF);
    }

    /* Get list of ratios to use for each increasing query weights */
    for (i = 0; i < num_best_ratio; i++) {
        (void) sprintf (param_buf, "feedback.%ld.", i);
        prefix = param_buf;
        if (UNDEF == lookup_spec_prefix (spec, &wspec_args[0], num_wspec_args))
            return (UNDEF);
        best_ratio[i] = current_best_ratio;
        PRINT_TRACE (8, print_float, &best_ratio[i]);
    }

    /* Call all initialization procedures */
    if (UNDEF == (contok_inst =
		      init_contok_dict_noinfo(spec, "ctype.2.")) ||
        UNDEF == (tokcon_inst =
		      tokcon->init_proc(spec, "index.section.word."))) {
        return (UNDEF);
    }
    lwp = (LISTWT *) NULL;
    num_lw = max_num_lw = 0;

    ip->valid_info++;

    PRINT_TRACE (2, print_string, "Trace: leaving init_best_ret_inst_coocc");
    return (new_inst);
}

int
best_ret_inst_coocc (fdbk_info, unused, inst)
FEEDBACK_INFO *fdbk_info;
char *unused;
int inst;
{
    STATIC_INFO *ip;
    long i,j;
    long status;

    PRINT_TRACE (2, print_string, "Trace: entering best_ret_inst_coocc");
    PRINT_TRACE (6, print_fdbk_info, fdbk_info);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "best_ret_inst_coocc");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Reset everything for this query */
     /* Ensure all values in full_results and top_results are 0 initially */
    bzero ((char *) full_results, 
           sizeof (float) * (int) (results.num_full_results));
    bzero ((char *) top_results, 
           sizeof (TOP_RESULT) * (int) (results.num_top_results+1));
    results.min_top_sim = 0.0;
    current_eval = 0.0;

    qrels.qid = fdbk_info->orig_query->id_num.id;
    if (UNDEF == seek_rr_vec(qrels_fd, &qrels) ||
        UNDEF == read_rr_vec(qrels_fd, &qrels))
        return (UNDEF);
    if (qrels.qid !=  fdbk_info->orig_query->id_num.id ||
        qrels.num_rr <= 0) {
        /* No relevance info.  Just return original query */
        for (i = 0; i < fdbk_info->num_occ; i++) {
            if (! fdbk_info->occ[i].query) 
                fdbk_info->occ[i].weight = 0.0;
            PRINT_TRACE (4, print_string, "No relevance info");
            PRINT_TRACE (4, print_fdbk_info, fdbk_info);
            PRINT_TRACE (2, print_string, "Trace: leaving best_ret_inst_coocc");
        }
    }

    /* Sort occ by orig_query, then num_rel, then weight */
    (void) qsort ((char *) fdbk_info->occ, fdbk_info->num_occ,
                  sizeof (OCC), compare_occ_info);

    /* perform original search with query weights, keeping track 
       of top docs.  Then sort top_results by docid */
    for (i = 0; i < fdbk_info->num_occ; i++) {
	if (fdbk_info->occ[i].weight > 0)
	    if (UNDEF == do_orig (fdbk_info->orig_query->id_num.id,
				  &fdbk_info->occ[i]))
		return (UNDEF);
    }

    for (i = 0; i < results.num_top_results; i++) {
        if (top_results[i].did == 0)
            break;
    }
    results.num_top_results = i;
    qsort ((char *) top_results, results.num_top_results,
           sizeof (TOP_RESULT), comp_did);

    /* Evaluate original query */
    (void) eval_top (&current_eval);

    /* Foreach best_ratio, foreach nonzero query term, increase 
       query term's weight by that percentage and see if that improves query */
    for (j = 0; j < num_best_ratio; j++) {
        for (i = 0; i < fdbk_info->num_occ; i++) {
            if (fdbk_info->occ[i].weight > 0.0) {
                if (UNDEF == (status =
			      try_improve(fdbk_info->orig_query->id_num.id,
					  &fdbk_info->occ[i],
					  fdbk_info->occ[i].weight *
					  best_ratio[j])))
                    return (UNDEF);
            }
        }
        PRINT_TRACE (8, print_string, "After trying ratio");
        PRINT_TRACE (8, print_float, &best_ratio[j]);
        PRINT_TRACE (8, print_fdbk_info, fdbk_info);
    }

    PRINT_TRACE (4, print_fdbk_info, fdbk_info);
    PRINT_TRACE (2, print_string, "Trace: leaving best_ret_inst_coocc");
    return (1);
}

/* Try to improve the current retrieval results by adding term given
   by occinfo, with weight given by try_weight.  If successful,
   replace occinfo->weight with try_weight, and replace results with
   the results calculated including the new weight.  If failure,
   replace occinfo->weight with fail_weight, and do nothing to results.
*/

/* Result data structures: */
/* long num_wanted; number of top docs to keep track of */
/* float *full_results;  sims for best query found so far */
/* TOP_RESULT *top_results; sims for top docs of best query found so far,
   sorted by docid */
/* float *min_top_sim;  minimum sim for above */
/* TOP_RESULT *current_new_results;  sims for docs including term under 
   consideration which are greater than min_top_sim, sorted by docid */
/* TOP_REL_RESULT *new_results;  Merge of top_results and current_results,
   adding relevance info. */
/* RR_VEC qrels;  Relevance info for this query, sorted by docid */


static int
try_improve (query_id, occinfo, try_incr_weight)
long query_id;
OCC *occinfo;
double try_incr_weight;
{
    INV inv;
    TOP_RESULT *current_ptr = current_new_results;
    TOP_RESULT *end_current_ptr;
    LISTWT *listwt_ptr, *end_listwt_ptr;
    TOP_RESULT *top_ptr, *end_top_ptr;
    RR_TUP *qrels_ptr, *end_qrels_ptr;
    TOP_REL_RESULT *new_ptr, *end_new_ptr;
    float new_eval;

    PRINT_TRACE (10, print_string, "Entering try_improve with");
    PRINT_TRACE (10, print_long, &occinfo->con);
    PRINT_TRACE (10, print_float, &try_incr_weight);

    if (try_incr_weight == 0.0)
        return (0);

    inv.id_num = occinfo->con;
    if (occinfo->ctype < 2) {
	if (1 != seek_inv (inv_fd[occinfo->ctype], &inv) ||
	    1 != read_inv (inv_fd[occinfo->ctype], &inv)) {
	    return (0);
	}
    }
    else {
	if (UNDEF == get_coocc_inv(&inv))
	    return UNDEF;
    }

    /* Establish current_new_results */
    end_listwt_ptr = &inv.listwt[inv.num_listwt];
    for (listwt_ptr = inv.listwt;
         listwt_ptr < end_listwt_ptr;
         listwt_ptr++) {

	/* skip D3 for Q1 and Q4, and D1 for Q5 */
#define HACK (query_id >= 1 && query_id <= 50 && listwt_ptr->list > 742202 && listwt_ptr->list < 1078513) || \
	(query_id == 62 && listwt_ptr->list > 1078512) || \
	(query_id == 128 && listwt_ptr->list > 1078512) || \
	(query_id == 148 && listwt_ptr->list > 1078512) || \
	(query_id >= 151 && query_id <= 200 && query_id != 180 && (listwt_ptr->list > 742202 && listwt_ptr->list < 1078513)) || \
	(query_id != 180 && listwt_ptr->list > 742202) || \
	(query_id >= 201 && query_id <= 250 && listwt_ptr->list <= 510832) || \
	(query_id >= 251 && query_id <= 300 && query_id != 282 && (listwt_ptr->list < 510833 || (listwt_ptr->list > 742202 && listwt_ptr->list < 1078513))) || \
	(query_id == 282 && (listwt_ptr->list < 510833 || listwt_ptr->list > 742202)) || \
	(query_id > 10000 && listwt_ptr->list < 1078513)


	if (HACK)
	    continue;
        current_ptr->sim = full_results[listwt_ptr->list] +
            try_incr_weight * listwt_ptr->wt;
        if (current_ptr->sim < results.min_top_sim)
            continue;
        current_ptr->did = listwt_ptr->list;
        current_ptr++;
        if (current_ptr - current_new_results == num_current_new_results) {
	    long offset = current_ptr - current_new_results;

            num_current_new_results += num_wanted;
            if (NULL == (current_new_results = (TOP_RESULT *)
                         realloc ((char *) current_new_results,
                                  (unsigned) (num_current_new_results) *
                                  sizeof (TOP_RESULT))) ||
                NULL == (new_results = (TOP_REL_RESULT *)
                         realloc ((char *) new_results,
                                  (unsigned) (num_current_new_results+num_wanted) *
                                  sizeof (TOP_REL_RESULT))))
                return (UNDEF);
	    current_ptr = current_new_results + offset;
        }
    }

    /* Merge current_new_results and top_results */
    end_current_ptr = current_ptr;
    current_ptr = current_new_results;
    top_ptr = top_results;
    end_top_ptr = &top_results[results.num_top_results];
    qrels_ptr = qrels.rr;
    end_qrels_ptr = &qrels.rr[qrels.num_rr];
    new_ptr = new_results;
    while (top_ptr < end_top_ptr && current_ptr < end_current_ptr) {
        if (top_ptr->did < current_ptr->did) {
            new_ptr->did = top_ptr->did;
            new_ptr->sim = top_ptr->sim;
            top_ptr++;
        }
        else if (top_ptr->did == current_ptr->did) {
            new_ptr->did = current_ptr->did;
            new_ptr->sim = current_ptr->sim;
            current_ptr++;
            top_ptr++;
        }
        else {
            new_ptr->did = current_ptr->did;
            new_ptr->sim = current_ptr->sim;
            current_ptr++;
        }
PRINT_TRACE (20, print_long, &qrels_ptr->did);
PRINT_TRACE (20, print_long, &new_ptr->did);
        while (qrels_ptr < end_qrels_ptr && qrels_ptr->did < new_ptr->did)
            qrels_ptr++;
PRINT_TRACE (20, print_long, &qrels_ptr->did);
PRINT_TRACE (20, print_long, &new_ptr->did);
        if (qrels_ptr < end_qrels_ptr && qrels_ptr->did == new_ptr->did)
            new_ptr->rel = 1;
        else
            new_ptr->rel = 0;
        new_ptr++;
    }
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
    while (current_ptr < end_current_ptr) {
        new_ptr->did = current_ptr->did;
        new_ptr->sim = current_ptr->sim;
        current_ptr++;
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

    (void) eval (new_ptr, new_results, &new_eval);
    PRINT_TRACE (10, print_float, &current_eval);
    PRINT_TRACE (10, print_float, &new_eval);
  
    /* If new_eval <= old_eval,do nothing */
    if (new_eval <= current_eval) {
        PRINT_TRACE (10, print_string, "try_improve: Returning after failure");
        return (0);
    }

    /* If new_eval > current_eval then redo similarity computation, this
       time storing results into full_results.  Replace top_results
       by the values in new_results. Set current_eval, occinfo->weight */
    end_listwt_ptr = &inv.listwt[inv.num_listwt];
    for (listwt_ptr = inv.listwt;
         listwt_ptr < end_listwt_ptr;
         listwt_ptr++) {
        full_results[listwt_ptr->list] +=
            try_incr_weight * listwt_ptr->wt;
    }

    top_ptr = top_results;
    end_new_ptr = new_ptr;
    new_ptr = new_results;
    while (new_ptr < end_new_ptr && top_ptr - top_results < num_wanted) {
        top_ptr->did = new_ptr->did;
        top_ptr->sim = new_ptr->sim;
        top_ptr++; new_ptr++;
    }

    if (new_ptr != new_results)
        results.min_top_sim = (new_ptr-1)->sim;

    qsort ((char *) top_results, top_ptr-top_results,
           sizeof (TOP_RESULT), comp_did);

    results.num_top_results = top_ptr-top_results;
    current_eval = new_eval;

    occinfo->weight += try_incr_weight;

    PRINT_TRACE (10, print_string, "try_improve: Returning after success");
    return (1);
}


int
close_best_ret_inst_coocc (inst)
int inst;
{
    STATIC_INFO *ip;
    PRINT_TRACE (2, print_string, "Trace: entering close_best_ret_inst_coocc");
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "ctype_inv");
        return (UNDEF);
    }
    ip  = &info[inst];
    ip->valid_info--;

    if (ip->valid_info == 0) {
        if (UNDEF == close_contok_dict_noinfo(contok_inst) ||
            UNDEF == tokcon->close_proc(tokcon_inst))
	    return UNDEF;
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_best_ret_inst_coocc");
    return (0);
}


/********************   PROCEDURE DESCRIPTION   ************************
*0 Keep track of top ranked documents during course of a retrieval.
*2 rank_trec_did (new, results, inst)
*3   TOP_RESULT *new;
*3   RESULT *results;
*3   int inst;
*7 Add the new document to results.top_results if the similarity is
*7 high enough.  Break ties among similarities by ranking the document
*7 with highest docid first.
*7 Assumes upon new query calling routine sets results->min_top_sim to 0.0 
*7 and zero's out the top_results array.
*7 Assumes sims in top_sims only increase (positive weights).
*9 Eventually, should do same as above.  (Keep track of current term's
*9 docs that exceed min_top_sim with a heap of size num_wanted, then
*9 merge at end).
***********************************************************************/


static long min_sim_index;

static int
rank_did (new, results, inst)
TOP_RESULT *new;
RESULT *results;
int inst;
{
    long num_wanted = results->num_top_results;

    TOP_RESULT *last_res = &(results->top_results[num_wanted]);

    TOP_RESULT *min_res;   /* Position of doc with current min sim */
    TOP_RESULT *new_min_res; /* Position of doc with new min sim */
    TOP_RESULT *temp_res;
    TOP_RESULT *save_res;
    TOP_RESULT save_min;   /* Saved copy of min_res */

    if (num_wanted <= 0)
        return (0);

    /* Reset min_sim_index if new query */
    if (results->min_top_sim == 0.0 && results->top_results[0].did == 0)
        min_sim_index = 0;

    min_res = &(results->top_results[min_sim_index]);

    /* Enter doc into top_results if similarity high enough. */
    if (new->sim < min_res->sim ||
        (new->sim == min_res->sim && 
         new->did <= min_res->did))
        /* Don't need to enter */
        return (0);

    save_min.did = min_res->did;
    save_min.sim = min_res->sim;

    min_res->sim = new->sim;
    min_res->did = new->did;

    new_min_res = results->top_results;
    save_res = (TOP_RESULT *) NULL;
    for (temp_res = results->top_results;
         temp_res < last_res;
         temp_res++) {
        if (new->did == temp_res->did && temp_res != min_res)
            save_res = temp_res;
        if (temp_res->sim < new_min_res->sim ||
            (temp_res->sim == new_min_res->sim &&
             temp_res->did < new_min_res->did)) {
            new_min_res = temp_res;
        }
    }

    if (save_res != (TOP_RESULT *) NULL) {
        /* New doc was already in top docs and now is represented twice */
        /* Restore old min_res in old location of new doc */
        /* Assumes new similarity for new doc is greater than old */
        /* similarity */
        save_res->sim = save_min.sim;
        save_res->did = save_min.did;
        if (save_res->sim < new_min_res->sim ||
            (save_res->sim == new_min_res->sim &&
             save_res->did < new_min_res->did)) {
            new_min_res = save_res;
        }
    }

    results->min_top_sim = new_min_res->sim;
    min_sim_index = new_min_res - results->top_results;

    return (1);
}


static int
do_orig (query_id, occinfo)
long query_id;
OCC *occinfo;
{
    INV inv;
    double query_weight = occinfo->weight;
    TOP_RESULT new_top;
    LISTWT *listwt_ptr, *end_listwt_ptr;

    inv.id_num = occinfo->con;
    if (occinfo->ctype < 2) {
	if (1 != seek_inv (inv_fd[occinfo->ctype], &inv) ||
	    1 != read_inv (inv_fd[occinfo->ctype], &inv)) {
	    return (0);
	}
    }
    else {
	if (UNDEF == get_coocc_inv(&inv))
	    return UNDEF;
    }

    end_listwt_ptr = &inv.listwt[inv.num_listwt];
    for (listwt_ptr = inv.listwt;
         listwt_ptr < end_listwt_ptr;
         listwt_ptr++) {
	if (HACK)
	    continue;
	full_results[listwt_ptr->list] += query_weight * listwt_ptr->wt;
	/* Add to top results (potentially) if new sim exceeds top_thresh*/
	/* (new_top points to last entry in top_results subarray of */
	/* results) */
	if (full_results[listwt_ptr->list] >=
	    results.min_top_sim) {
	    new_top.sim = results.full_results[listwt_ptr->list];
	    new_top.did = listwt_ptr->list;
	    if (UNDEF == rank_did (&new_top, &results, 0))
		return (UNDEF);
	}
    }
    return (1);
}

static int
eval (new_ptr,  new_results, new_eval)
TOP_REL_RESULT *new_ptr;
TOP_REL_RESULT *new_results;
float *new_eval;
{
    long num_rel, num_nonrel;
    TOP_REL_RESULT *end_new_ptr;

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
	}
	else
	    num_nonrel++;
	new_ptr++;
    }
    *new_eval = *new_eval / qrels.num_rr;

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
get_coocc_inv(inv)
INV *inv;
{
    char term_buf[PATH_LEN], *term_pair, *term1, *term2;
    long con1, con2;
    INV inv1, inv2;
    register LISTWT *lwp1, *end1, *lwp2, *end2;

    /* Get the term-pair corresponding to this ctype 2 term */
    if (UNDEF == contok_dict_noinfo(&inv->id_num, &term_pair, contok_inst))
	return(UNDEF);
    strcpy(term_buf, term_pair);
    term2 = term1 = term_buf;
    while (*term2 != ' ') term2++;
    *term2 = '\0';
    term2++;
    if (UNDEF == tokcon->proc(term1, &con1, tokcon_inst) ||
	UNDEF == tokcon->proc(term2, &con2, tokcon_inst))
	return(UNDEF);

    /* Get the inverted lists corresponding to the 2 terms */
    inv1.id_num = con1;
    if (1 != seek_inv(inv_fd[0], &inv1) ||
	1 != read_inv(inv_fd[0], &inv1)) {
        PRINT_TRACE (1, print_string, "occ_info: concept not found in inv_file:");
        PRINT_TRACE (1, print_long, &con1);
	inv->num_listwt = 0;
	return (0);
    }
    inv2.id_num = con2;
    if (1 != seek_inv(inv_fd[0], &inv2) ||
	1 != read_inv(inv_fd[0], &inv2)) {
	printf("Inverted list for concept %ld not found\n", con2);
	inv->num_listwt = 0;
	return (0);
    }

    if (max_num_lw < MAX(inv1.num_listwt, inv2.num_listwt)) {
	if (max_num_lw)
	    Free(lwp);
	max_num_lw = MAX(inv1.num_listwt, inv2.num_listwt);
	if (NULL == (lwp = (LISTWT *) 
		     malloc ((unsigned) (max_num_lw) * sizeof (LISTWT))))
	    return UNDEF;
    }

    num_lw = 0;
    end1 = &inv1.listwt[inv1.num_listwt];   
    end2 = &inv2.listwt[inv2.num_listwt];
    for (lwp1 = inv1.listwt, lwp2 = inv2.listwt;
	 lwp1 < end1 && lwp2 < end2;) {
	/* find common docs */
	if (lwp1->list < lwp2->list) {
	    lwp1++; continue;
	}
	if (lwp1->list > lwp2->list) {
	    lwp2++;
	    continue;
	}
	lwp[num_lw].list = lwp1->list;
	lwp[num_lw].wt = MIN(lwp1->wt, lwp2->wt);
	num_lw++;
	lwp1++;
	lwp2++;
    }

    inv->num_listwt = num_lw;
    inv->listwt = lwp;

    return 0;
}


static int
compare_occ_info (ptr1, ptr2)
OCC *ptr1;
OCC *ptr2;
{
    if (ptr1->query && !ptr2->query)
        return (-1);
    if (!ptr1->query && ptr2->query)
        return (1);

    if (ptr1->ctype < ptr2->ctype)
        return (-1);
    if (ptr1->ctype > ptr2->ctype)
        return (1);

    if (ptr1->weight < ptr2->weight)
        return (-1);
    if (ptr1->weight > ptr2->weight)
        return (1);

    return (ptr1->con - ptr2->con);
}

/*********************************************************
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
    if (ptr1->rel_weight > ptr2->rel_weight)
        return (-1);
    if (ptr1->rel_weight < ptr2->rel_weight)
        return (1);
    return (0);
}
*********************************************************/


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

