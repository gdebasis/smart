#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libevaluate/rr_eval.c,v 11.0 1992/07/21 18:20:34 chrisb Exp chrisb $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Given retrieval specs, evaluate each query using ranks of top retrieved docs
 *2 tr_eval_list (spec_list, eval_list_list, inst)
 *3   SPEC_LIST *spec_list;
 *3   EVAL_LIST_LIST *eval_list_list;
 *3   int inst;
 *4 init_tr_eval_list (spec, unused)
 *4   "eval.tr_file.rmode"
 *4   "eval.qrels_file"
 *4   "eval.qrels_file.rmode"
 *5   "eval.trace"
 *4 close_tr_eval_list (inst)
 *7 Evaluate all the retrieval runs given in spec_list.  The list of relevant
 *7 retrieved docs for each run is given by the tr_file parameter of each
 *7 spec list spec file, along with the total number of retrieved docs.  
 *7 The total number of relevant docs for each query is given by qrels.
 *7 The end result is X evaluations of runs (where X is the number of spec
 *7 files with valid results (rr_file)), where each evaluation is an
 *7 EVAL_LIST consisting of N query evaluations, each one being an EVAL object.
 *7 Note that qrels_file, and the parameters for the evaluation
 *7 itself (eg, number of cutoff points within q_eval), are all set globally
 *7 using the init_tr_eval_list spec file, while the retrieval run info is
 *7 taken from the individual tr_eval_list spec files.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "io.h"
#include "rel_header.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "tr_vec.h"
#include "rr_vec.h"
#include "eval.h"
#include "inst.h"

void print_eval_list();

static char *qrels_file;
static long qrels_mode;
static long tr_mode;

static SPEC_PARAM spec_args[] = {
    {"eval.qrels_file",          getspec_dbfile, (char *) &qrels_file},
    {"eval.qrels_file.rmode",    getspec_filemode, (char *) &qrels_mode},
    {"eval.tr_file.rmode",     getspec_filemode, (char *) &tr_mode},
    TRACE_PARAM ("eval.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static char *run_name;
static char *tr_file;
static SPEC_PARAM tr_spec_args[] = {
    {"eval.run_name",       getspec_string, (char *) &run_name},
    {"eval.tr_file",        getspec_dbfile, (char *) &tr_file},
    };
static int num_tr_spec_args = sizeof (tr_spec_args) /
         sizeof (tr_spec_args[0]);


/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    long tr_mode;
    int qeval_inst;

    long *rel_ret_rank;                 /* Space for the current query's */
    long max_rel_ret_rank;              /* relevant document ranks */

    EVAL_LIST_LIST eval_list_list;       /* Pointers to all the malloc'd
                                            space (for eventual freeing) */
    long num_docs;
    long num_num_rels;                 /* Number of queries with rel docs. */
    long *num_rels;                    /* Number of rel docs for each query */
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

#define INIT_REL_RET_RANK 100
#define INIT_NUM_QUERY 100
#define INIT_NUM_EVAL_LIST 1

static int compare_long();

int
init_tr_eval_list (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;
    long i;
    int qrels_fd;
    RR_VEC qrels;

    REL_HEADER *rh;
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_tr_eval_list");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    ip = &info[new_inst];
    
   /* Need to get the number of queries with rel docs in the collection */
    if (! VALID_FILE (qrels_file) ||
        NULL == (rh = get_rel_header (qrels_file))) {
        set_error (SM_ILLPA_ERR, qrels_file, "init_tr_eval_list");
        return (UNDEF);
    }
    ip->num_num_rels = rh->max_primary_value + 1;
    if (NULL == (ip->num_rels = (long *)
                 malloc ((unsigned) ip->num_num_rels * sizeof (long))))
        return (UNDEF);
    (void) bzero ((char *) ip->num_rels, ip->num_num_rels * sizeof (long));

    /* Actually find the number of rel docs for each potential query */
    if (UNDEF == (qrels_fd = open_rr_vec (qrels_file, qrels_mode)))
        return (UNDEF);
    ip->max_rel_ret_rank = 0;
    while (1 == read_rr_vec (qrels_fd, &qrels)) {
        ip->num_rels[qrels.qid] = qrels.num_rr;
        if (qrels.num_rr > ip->max_rel_ret_rank)
            ip->max_rel_ret_rank = qrels.num_rr;
    }
    if (UNDEF == close_rr_vec (qrels_fd))
        return (UNDEF);

    /* Initialize per query evaluation procedure */
    if (UNDEF == (ip->qeval_inst = init_q_eval (spec, (char *) NULL)))
        return (UNDEF);

    /* Reserve space for intermediate and end results */
    if (NULL == (ip->rel_ret_rank = (long *)
                 malloc (ip->max_rel_ret_rank * sizeof (long))) ||
        NULL == (ip->eval_list_list.eval_list = (EVAL_LIST *)
                 malloc (INIT_NUM_EVAL_LIST * sizeof (EVAL_LIST))))
        return (UNDEF);

    ip->eval_list_list.num_eval_list = INIT_NUM_EVAL_LIST;

    for (i = 0; i < ip->eval_list_list.num_eval_list; i++) {
        if (NULL == (ip->eval_list_list.eval_list[i].eval = (EVAL *)
                     malloc (INIT_NUM_QUERY * sizeof (EVAL))))
            return (UNDEF);
        ip->eval_list_list.eval_list[i].num_eval = INIT_NUM_QUERY;
    }

    ip->tr_mode = tr_mode;

    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_tr_eval_list");

    return (new_inst);
}

int
tr_eval_list (spec_list, eval_list_list, inst)
SPEC_LIST *spec_list;
EVAL_LIST_LIST *eval_list_list;
int inst;
{
    STATIC_INFO *ip;
    REL_HEADER *rh;
    long i;
    TR_VEC tr_vec;
    int tr_fd;
    long num_query;
    int status;
    EVAL_INPUT eval_input;
    EVAL_LIST *eval_list;
    long num_run;
    long num_rel_ret;

    PRINT_TRACE (2, print_string, "Trace: entering tr_eval_list");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "tr_eval_list");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Make sure enough space is reserved for the eval_lists */
    if (spec_list->num_spec > ip->eval_list_list.num_eval_list) {
        if (NULL == (ip->eval_list_list.eval_list = (EVAL_LIST *)
                     realloc ((char *)ip->eval_list_list.eval_list,
                              spec_list->num_spec * sizeof (EVAL_LIST))))
            return (UNDEF);
        for (i=ip->eval_list_list.num_eval_list; i < spec_list->num_spec;i++) {
            if (NULL == (ip->eval_list_list.eval_list[i].eval = (EVAL *)
                         malloc (INIT_NUM_QUERY * sizeof (EVAL))))
                return (UNDEF);
            ip->eval_list_list.eval_list[i].num_eval = INIT_NUM_QUERY;
        }
        ip->eval_list_list.num_eval_list = spec_list->num_spec;
    }

    for (num_run = 0; num_run < spec_list->num_spec; num_run++) {
        eval_list = &ip->eval_list_list.eval_list[num_run];

        /* Lookup the value of tr_file in the incoming spec file */
        if (UNDEF == lookup_spec (spec_list->spec[num_run],
                                  &tr_spec_args[0], num_tr_spec_args) ||
            (! VALID_FILE (tr_file)) || 
            (NULL == (rh = get_rel_header (tr_file))) ||
            rh->num_entries == 0) {
            clr_err();
            eval_list->description = "tr_file nonexistent or empty";
	    eval_list->num_eval = 0;
            continue;
        }
        
        if (rh->num_entries > eval_list->num_eval) {
            eval_list->num_eval = rh->num_entries;
            if (NULL == (eval_list->eval = (EVAL *)
                         realloc ((char *) eval_list->eval,
                             (unsigned) eval_list->num_eval * sizeof (EVAL))))
                return (UNDEF);
        }
        
        if (UNDEF == (tr_fd = open_tr_vec (tr_file, ip->tr_mode))) {
            return (UNDEF);
        }
        
        num_query = 0;
        while (1 == (status = read_tr_vec (tr_fd, &tr_vec))) {
            /* Ignore queries with no rel docs */
            if (ip->num_rels[tr_vec.qid] <= 0)
                continue;

            num_rel_ret = 0;
            for (i = 0; i < tr_vec.num_tr; i++) {
                if (tr_vec.tr[i].rel > 0) {
                    if (ip->num_num_rels < tr_vec.qid ||
                        num_rel_ret >= ip->num_rels[tr_vec.qid]) {
                        set_error (SM_INCON_ERR,
                                   "More retrieved rel docs than known rel",
                                   "tr_eval_list");
                        return (UNDEF);
                    }
                    ip->rel_ret_rank[num_rel_ret] = tr_vec.tr[i].rank;
                    num_rel_ret++;
                }
            }
            /* Sort by increasing rank */
            qsort ((char *) ip->rel_ret_rank,
                   (int) num_rel_ret,
                   sizeof (long),
                   compare_long);

            eval_input.rel_ret_rank = ip->rel_ret_rank;
            eval_input.qid = tr_vec.qid;
            eval_input.num_rel_ret = num_rel_ret;
            if (tr_vec.qid < ip->num_num_rels)
                eval_input.num_rel = ip->num_rels[tr_vec.qid];
            else
                eval_input.num_rel = 0;
            eval_input.num_ret = tr_vec.num_tr;
            /* Perform the evaluation */
            /* WARNING: Assumes that q_eval will not overwrite the
               previous q_eval's calls data (locally malloc'd space).
               This is a legacy from previous versions in which EVAL had
               no pointers.
               */
            if (UNDEF == q_eval (&eval_input, &eval_list->eval[num_query],
                                 ip->qeval_inst))
                return (UNDEF);
            num_query++;
        }
        if (status == UNDEF)
            return (UNDEF);
        
        eval_list->description = run_name;
        
        eval_list->num_eval = num_query;
        
        if (UNDEF == close_tr_vec (tr_fd))
            return (UNDEF);
        
        PRINT_TRACE (4, print_eval_list, eval_list);
    }

    eval_list_list->eval_list = ip->eval_list_list.eval_list;
    eval_list_list->num_eval_list = num_run;
    eval_list_list->description = "    Evaluation of Top Retrieved Documents";

    PRINT_TRACE (2, print_string, "Trace: leaving tr_eval_list");
    return (1);
}


int
close_tr_eval_list (inst)
int inst;
{
    STATIC_INFO *ip;
    long i;

    PRINT_TRACE (2, print_string, "Trace: entering close_tr_eval_list");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "tr_eval_list");
        return (UNDEF);
    }
    ip  = &info[inst];

    ip->valid_info--;

    (void) free ((char *) ip->rel_ret_rank);
    if (UNDEF == close_q_eval (ip->qeval_inst))
        return (UNDEF);

    for (i = 0; i < ip->eval_list_list.num_eval_list; i++) {
        (void) free ((char *) ip->eval_list_list.eval_list[i].eval);
    }
    (void) free ((char *) ip->eval_list_list.eval_list);

    PRINT_TRACE (2, print_string, "Trace: leaving close_tr_eval_list");
    return (0);
}

static int
compare_long (long1, long2)
long *long1;
long *long2;
{
    if (*long1 < *long2)
        return (-1);
    if (*long1 > *long2)
        return (1);
    return (0);
}
