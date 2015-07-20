#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libproc/proc_eval.c,v 11.2 1993/02/03 16:53:37 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "proc.h"

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table giving which evaluation results to prepare for printing
 *1 evaluate.eval_convert
 *2 * (eval_list_list, result_eval_list)
 *3   EVAL_LIST_LIST eval_list_list;
 *3   EVAL_LIST result_eval_list;
 *3   int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Return UNDEF if error.

 *8 Current possibilities are "avg", "dev", and "comp_over"
***********************************************************************/
extern int init_ell_el_avg(), ell_el_avg(), close_ell_el_avg();
extern int init_ell_el_dev(), ell_el_dev(), close_ell_el_dev();
extern int init_ell_el_comp_over(), ell_el_comp_over(), close_ell_el_comp_over();
extern int init_ell_el_comp_over_equal(), ell_el_comp_over_equal(), close_ell_el_comp_over_equal();
extern int init_eval_mrr_trvec(), eval_mrr_trvec(), close_eval_mrr_trvec();

static PROC_TAB proc_eval_convert[] = {
    {"avg",	init_ell_el_avg,	ell_el_avg,	close_ell_el_avg},
    {"dev",	init_ell_el_dev,	ell_el_dev,	close_ell_el_dev},
    {"comp_over",init_ell_el_comp_over,ell_el_comp_over,close_ell_el_comp_over},
    {"comp_over_equal",init_ell_el_comp_over_equal,ell_el_comp_over_equal,close_ell_el_comp_over_equal},
    };
static int num_proc_eval_convert = sizeof (proc_eval_convert) / sizeof (proc_eval_convert[0]);

extern int init_print_obj_eval(), print_obj_eval(), close_print_obj_eval();
static PROC_TAB proc_print[] = {
    {"print_obj_eval",init_print_obj_eval,print_obj_eval,close_print_obj_eval},
    {"print_mrr_eval",init_eval_mrr_trvec, eval_mrr_trvec, close_eval_mrr_trvec},
};
static int num_proc_print = sizeof (proc_print) / sizeof (proc_print[0]);

TAB_PROC_TAB proc_eval[] = {
    {"eval_convert",   TPT_PROC, NULL, proc_eval_convert,     &num_proc_eval_convert},
    {"print",       TPT_PROC, NULL, proc_print,          &num_proc_print},
    };

int num_proc_eval = sizeof (proc_eval) / sizeof (proc_eval[0]);
