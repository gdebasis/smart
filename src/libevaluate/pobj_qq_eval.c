#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libevaluate/rrvec_trec_eval.c,v 11.0 1992/07/21 18:20:34 chrisb Exp chrisb $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Print evaluation results for a set of retrieval runs
 *2 print_obj_qq_eval (in_file, out_file, inst)
 *3   char *in_file;
 *3   char *out_file;
 *3   int inst;
 *4 init_print_obj_qq_eval (spec, unused)
 *5   "eval.trace"
 *5   "eval.eval_convert"
 *5   "eval.eval_type"
 *5   "eval.verbose"
 *4 close_print_obj_qq_eval (inst)
 *7 Calculate and print various evaluation measures for a set of runs.
 *7 in_file is a list of whitespace separated run specification files.
 *7 If not VALID_FILE, then default is to use the smart invocation
 *7 spec file.  Output to go into file "out_file" (if not VALID_FILE,
 *7 then stdout).
 *7 eval_type should be either "rr" or "tr" (default "rr") to
 *7    indicate source of evaluation data (tr_file or rr_file)
 *7 eval.verbose gives which eval results should be printed.  0 indicates
 *7    print a single line giving average precision at all recall points for
 *7    each run.  1 indicates the major results should be printed (the
 *7    TREC 2/3 official results).  Greater than 1 indicates all results.
 *7    Standard SMART default for "verbose" is 1.
 *7 This is a variant of the standard print_obj_eval routine (see pobj_eval.c)
 *7 that prints out results on a per-query basis. 
 *7 Thus, the eval_convert procedure is no longer necessary, since no
 *7 averaging over queries is done and the results for individual queries
 *7 are printed out.
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

static long verbose;

static SPEC_PARAM spec_args[] = {
    {"eval.verbose",     getspec_long, (char *) &verbose},
    TRACE_PARAM ("eval.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static char *datatype;

static SPEC_PARAM spec_args1[] = {
    {"eval.eval_type",     getspec_string, (char *) &datatype},
    };
static int num_spec_args1 = sizeof (spec_args1) /
         sizeof (spec_args1[0]);


/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    void (*print_proc)();
    int datatype;      /* RR_DATATYPE or TR_DATATYPE */
    int type_inst;     /* instantiation for either rr_eval_list or tr_eval_list
                          depending on datatype */

    SPEC *save_spec;             /* Pointer to original spec file */
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

#define RR_DATATYPE 1
#define TR_DATATYPE 0

int
init_print_obj_qq_eval (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;
    
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering/leaving init_print_obj_qq_eval");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    ip = &info[new_inst];


    if (verbose == 0)
        ip->print_proc = print_prec_eval_list;
    else if (verbose == 1)
        ip->print_proc = print_short_eval_list;
    else 
        ip->print_proc = print_eval_list;

    if (UNDEF == lookup_spec (spec, &spec_args1[0], num_spec_args1)) {
        ip->datatype = RR_DATATYPE;
        clr_err();
    }
    else if (0 == strcmp ("tr", datatype))
        ip->datatype = TR_DATATYPE;
    else
        ip->datatype = RR_DATATYPE;

    if (ip->datatype == TR_DATATYPE) {
        if (UNDEF == (ip->type_inst = init_tr_eval_list (spec, (char *) NULL)))
            return (UNDEF);
    }
    else {
        if (UNDEF == (ip->type_inst = init_rr_eval_list (spec, (char *) NULL)))
            return (UNDEF);
    }

    ip->save_spec = spec;
    ip->valid_info = 1;
    
    PRINT_TRACE (2, print_string, "Trace: leaving init_print_obj_qq_eval");
    return (new_inst);
}


int
print_obj_qq_eval (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
    char *desc_buf;
    long num_runs, num_queries, qid, desc_buf_size, buf_offset, q, r;
    SPEC_LIST spec_list;
    FILE *output;
    SM_BUF output_buf;
    STATIC_INFO *ip;
    EVAL_LIST_LIST eval_list_list;
    EVAL_LIST result_eval_list, *el;

    PRINT_TRACE (2, print_string, "Trace: entering print_obj_qq_eval");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "print_obj_qq_eval");
        return (UNDEF);
    }
    ip  = &info[inst];


    PRINT_TRACE (2, print_string, "Trace: entering print_obj_tr_eval");

    if (VALID_FILE (in_file)) {
        if (UNDEF == get_spec_list (in_file, &spec_list))
            return (UNDEF);
    }
    else {
        spec_list.spec = &ip->save_spec;
        spec_list.spec_name = (char **) NULL;
        spec_list.num_spec = 1;
    }

    output = VALID_FILE (out_file) ? fopen (out_file, "w") : stdout;
    if (NULL == output)
        return (UNDEF);
    output_buf.size = 0;
    output_buf.end = 0;

    if (ip->datatype == TR_DATATYPE) {
        if (UNDEF == tr_eval_list (&spec_list,
                                   &eval_list_list,
                                   ip->type_inst))
            return (UNDEF);
    }
    else {
        if (UNDEF == rr_eval_list (&spec_list,
                                   &eval_list_list,
                                   ip->type_inst))
            return (UNDEF);
    }

    if (eval_list_list.num_eval_list == 0) {
	if (UNDEF == add_buf_string("No results to show\n", &output_buf))
	    return(UNDEF);
	return (0);
    }
    num_runs = eval_list_list.num_eval_list;
    num_queries = eval_list_list.eval_list[0].num_eval; 
    result_eval_list.num_eval = num_runs;
    /* Handle run description */
    desc_buf_size = 3 + strlen(eval_list_list.description);
    if (NULL == (desc_buf = Malloc(desc_buf_size, char)))
	return(UNDEF);
    sprintf(desc_buf, "%s\n\n", eval_list_list.description); 
    buf_offset = strlen(desc_buf);
    if (NULL == (result_eval_list.eval = Malloc(num_runs, EVAL)))
	return(UNDEF);
    el = eval_list_list.eval_list;
    for (q = 0; q < num_queries; q++) {
	qid = el[0].eval[q].qid;
	for (r = 0; r < num_runs; r++) {
	    if (el[r].eval[q].qid != qid) {
		set_error(SM_INCON_ERR, "QID mismatch", "print_obj_qq_eval");
		return(UNDEF);
	    }
	    result_eval_list.eval[r] = el[r].eval[q];
	    if (6+strlen(desc_buf)+strlen(el[r].description) > desc_buf_size) {
		desc_buf_size += 6 + strlen(el[r].description) * num_runs;
		if (NULL == (desc_buf = Realloc(desc_buf, desc_buf_size, char)))
		    return(UNDEF);
	    }
	    sprintf(&(desc_buf[strlen(desc_buf)]), "%3ld. %s\n", 
		    r, el[r].description);
	}
	result_eval_list.description = desc_buf;
        ip->print_proc (&result_eval_list, &output_buf);
	desc_buf[buf_offset] = '\0';
    }
    Free(result_eval_list.eval);

    (void) fwrite (output_buf.buf, 1, output_buf.end, output);

    if (output != stdin)
        (void) fclose (output);

    PRINT_TRACE (2, print_string, "Trace: leaving print_obj_qq_eval");
    return (1);
}


int
close_print_obj_qq_eval (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_print_obj_qq_eval");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_print_obj_qq_eval");
        return (UNDEF);
    }
    ip  = &info[inst];

    ip->valid_info--;

    if (ip->datatype == TR_DATATYPE) {
        if (UNDEF == close_tr_eval_list (ip->type_inst))
            return (UNDEF);
    }
    else {
        if (UNDEF == close_rr_eval_list (ip->type_inst))
            return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_print_obj_qq_eval");
    return (0);
}
