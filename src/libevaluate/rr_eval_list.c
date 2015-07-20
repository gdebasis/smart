#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libevaluate/rr_eval.c,v 11.0 1992/07/21 18:20:34 chrisb Exp chrisb $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Given retrieval specs, evaluate each query using ranks of relevant docs
 *2 rr_eval_list (spec_list, eval_list_list, inst)
 *3   SPEC_LIST *spec_list;
 *3   EVAL_LIST_LIST *eval_list_list;
 *3   int inst;
 *4 init_rr_eval_list (spec, unused)
 *4   "eval.doc_file"
 *4   "eval.doc.textloc_file"
 *4   "eval.rr_file.rmode"
 *5   "eval.trace"
 *4 close_rr_eval_list (inst)
 *7 Evaluate all the retrieval runs given in spec_list.  The list of relevant
 *7 docs for each run is given by the rr_file parameter of the spec file, with
 *7 number of retrieved docs being set to the total number of docs in
 *7 the collection using either doc_file or textloc_file.
 *7 The end result is X evaluations of runs (where X is the number of spec
 *7 files with valid results (rr_file)), where each evaluation is an
 *7 EVAL_LIST consisting of N query evaluations, each one being an EVAL object.
 *7 Note that doc_file, textloc_file, and the parameters for the evaluation
 *7 itself (eg, number of cutoff points within q_eval), are all set globally
 *7 using the init_rr_eval_list spec file, while the retrieval run info is
 *7 taken from the individual rr_eval_list spec files.
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
#include "rr_vec.h"
#include "eval.h"
#include "inst.h"

void print_eval_list();

static char *doc_file;
static char *textloc_file;
static long rr_mode;

static SPEC_PARAM spec_args[] = {
    {"eval.doc_file",        getspec_dbfile, (char *) &doc_file},
    {"eval.doc.textloc_file",    getspec_dbfile, (char *) &textloc_file},
    {"eval.rr_file.rmode",     getspec_filemode, (char *) &rr_mode},
    TRACE_PARAM ("eval.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static char *run_name;
static char *rr_file;
static SPEC_PARAM rr_spec_args[] = {
    {"eval.run_name",       getspec_string, (char *) &run_name},
    {"eval.rr_file",        getspec_dbfile, (char *) &rr_file},
    };
static int num_rr_spec_args = sizeof (rr_spec_args) /
         sizeof (rr_spec_args[0]);


/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    long rr_mode;
    int qeval_inst;

    long *rel_ret_rank;                 /* Space for the current query's */
    long max_rel_ret_rank;              /* relevant document ranks */

    EVAL_LIST_LIST eval_list_list;       /* Pointers to all the malloc'd
                                            space (for eventual freeing) */
    long num_docs;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

#define INIT_REL_RET_RANK 100
#define INIT_NUM_QUERY 100
#define INIT_NUM_EVAL_LIST 1

static int compare_long();

int
init_rr_eval_list (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;
    long i;

    REL_HEADER *rh;
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_rr_eval_list");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    ip = &info[new_inst];
    
   /* Need to get the number of docs in the collection for num retrieved */
    rh = (REL_HEADER *) NULL;
    if (VALID_FILE (textloc_file)) {
        if (NULL == (rh = get_rel_header (textloc_file))) {
            set_error (SM_ILLPA_ERR, textloc_file, "init_rr_eval_list");
            return (UNDEF);
        }
    }
    else if (VALID_FILE (doc_file)) {
        if (NULL == (rh = get_rel_header (doc_file))) {
            set_error (SM_ILLPA_ERR, doc_file, "init_rr_eval_list");
            return (UNDEF);
        }
    }
    if (rh ==(REL_HEADER *) NULL)
        ip->num_docs = 0;
    else
        ip->num_docs = rh->num_entries;

    if (UNDEF == (ip->qeval_inst = init_q_eval (spec, (char *) NULL)))
        return (UNDEF);

    if (NULL == (ip->rel_ret_rank = (long *)
                 malloc (INIT_REL_RET_RANK * sizeof (long))) ||
        NULL == (ip->eval_list_list.eval_list = (EVAL_LIST *)
                 malloc (INIT_NUM_EVAL_LIST * sizeof (EVAL_LIST))))
        return (UNDEF);

    ip->max_rel_ret_rank = INIT_REL_RET_RANK;
    ip->eval_list_list.num_eval_list = INIT_NUM_EVAL_LIST;

    for (i = 0; i < ip->eval_list_list.num_eval_list; i++) {
        if (NULL == (ip->eval_list_list.eval_list[i].eval = (EVAL *)
                     malloc (INIT_NUM_QUERY * sizeof (EVAL))))
            return (UNDEF);
        ip->eval_list_list.eval_list[i].num_eval = INIT_NUM_QUERY;
    }

    ip->rr_mode = rr_mode;

    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_rr_eval_list");

    return (new_inst);
}

int
rr_eval_list (spec_list, eval_list_list, inst)
SPEC_LIST *spec_list;
EVAL_LIST_LIST *eval_list_list;
int inst;
{
    STATIC_INFO *ip;
    REL_HEADER *rh;
    long i;
    RR_VEC rr_vec;
    int rr_fd;
    long num_query;
    int status;
    EVAL_INPUT eval_input;
    EVAL_LIST *eval_list;
    long num_run;

    PRINT_TRACE (2, print_string, "Trace: entering rr_eval_list");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "rr_eval_list");
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

        /* Lookup the value of rr_file in the incoming spec file */
        if (UNDEF == lookup_spec (spec_list->spec[num_run],
                                  &rr_spec_args[0], num_rr_spec_args) ||
            (! VALID_FILE (rr_file)) || 
            (NULL == (rh = get_rel_header (rr_file))) ||
            rh->num_entries == 0) {
            clr_err();
            eval_list->description = "rr_file nonexistent or empty";
            continue;
        }
        
        if (rh->num_entries > eval_list->num_eval) {
            eval_list->num_eval = rh->num_entries;
            if (NULL == (eval_list->eval = (EVAL *)
                         realloc ((char *) eval_list->eval,
                             (unsigned) eval_list->num_eval * sizeof (EVAL))))
                return (UNDEF);
        }
        
        if (UNDEF == (rr_fd = open_rr_vec (rr_file, ip->rr_mode))) {
            return (UNDEF);
        }
        
        num_query = 0;
        while (1 == (status = read_rr_vec (rr_fd, &rr_vec))) {
            /* Reserve space for list of relevant retrieved docs */
            if (rr_vec.num_rr > ip->max_rel_ret_rank) {
                ip->max_rel_ret_rank += rr_vec.num_rr;
                if (NULL == (ip->rel_ret_rank = (long *)
                             realloc (ip->rel_ret_rank,
                                      sizeof (long) * ip->max_rel_ret_rank)))
                    return (UNDEF);
            }
            for (i = 0; i < rr_vec.num_rr; i++) {
                ip->rel_ret_rank[i] = rr_vec.rr[i].rank;
            }
            /* Sort by increasing rank */
            qsort ((char *) ip->rel_ret_rank,
                   (int) rr_vec.num_rr,
                   sizeof (long),
                   compare_long);

            eval_input.rel_ret_rank = ip->rel_ret_rank;
            eval_input.qid = rr_vec.qid;
            eval_input.num_rel_ret = rr_vec.num_rr;
            eval_input.num_rel = rr_vec.num_rr;
            eval_input.num_ret = ip->num_docs;
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
        
        if (UNDEF == close_rr_vec (rr_fd))
            return (UNDEF);
        
        PRINT_TRACE (4, print_eval_list, eval_list);
    }

    eval_list_list->eval_list = ip->eval_list_list.eval_list;
    eval_list_list->num_eval_list = num_run;
    eval_list_list->description = "    Evaluation of All Relevant Documents";

    PRINT_TRACE (2, print_string, "Trace: leaving rr_eval_list");
    return (1);
}


int
close_rr_eval_list (inst)
int inst;
{
    STATIC_INFO *ip;
    long i;

    PRINT_TRACE (2, print_string, "Trace: entering close_rr_eval_list");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "rr_eval_list");
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

    PRINT_TRACE (2, print_string, "Trace: leaving close_rr_eval_list");
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
