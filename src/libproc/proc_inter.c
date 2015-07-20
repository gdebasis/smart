#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libproc/proc_inter.c,v 11.2 1993/02/03 16:53:43 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Hierarchy table giving interactive top level procedures
 *1 top
 *2 * (unused1, unused2, inst)
 *3    char *unused1;
 *3    char *unused2;
 *3    int inst;
 *4 init_* (spec, unused)
 *4 close_* (inst)
 *7 Procedure table mapping a string procedure name to procedure addresses.
 *7 As top-level procedures, these procedures are responsible for setting
 *7 trace conditions, and for determining other execution time constraints,
 *7 such as when execution should stop (eg, if global_end is exceeded).
 *7 These procedures are the main user interactive procedures, with all
 *7 sorts of user-level commands available.
 *7 Return UNDEF if error, else 1.
 *8 Current possibilities are "inter", "X"
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "proc.h"

extern int init_inter(), inter(), close_inter();
static PROC_TAB proc_topinter[] = {
    {"inter",        init_inter,     inter,         close_inter},
    {"get_local_procs", INIT_EMPTY, EMPTY, CLOSE_EMPTY},
};
static int num_proc_topinter =
    sizeof (proc_topinter) / sizeof (proc_topinter[0]);



/* Command procedures */
int quit_inter(), exit_inter();
int help_message();
int init_show_titles(), inter_show_titles(), close_show_titles();
int init_show_doc(), close_show_doc(), show_next_doc(), show_named_doc(),
    set_raw_doc(), set_format_doc(), show_current_doc(), show_query();
int init_show_high_doc(), close_show_high_doc(), show_high_next_doc(), 
    show_high_named_doc(), show_high_current_doc();
int init_show_vec(), show_dvec(), show_qvec(), close_show_vec();
int init_show_inv(), show_inv(), close_show_inv();
int init_run_vec(), run_qvec(), close_run_vec(), run_new(), run_feedback(),
    run_dvec(), run_with_feedback();
int init_inter_save_file(), inter_save_file(), close_inter_save_file();
int init_show_raw(), close_show_raw(), show_raw_tr(), 
    show_spec(), show_raw_textloc();
int init_sim_vec(), sim_qvec(), sim_dvec(), close_sim_vec();
int init_match_vec(), match_qvec(), match_dvec(), close_match_vec();
int EMPTY();

int init_spec_list(), show_spec_list(), add_spec_list(), use_spec_list();
int init_set_query(), set_query(), close_set_query();
int init_show_exp(), close_show_exp(), show_exp_rr(), show_exp_tr(),
    show_exp_eval(), show_exp_q_eval(), show_exp_rclprn(),
    show_exp_ind_rr(), show_exp_ind_tr();

/* Following should be in liblocal?? */
/*
int init_trec_pager(), trec_pager(), close_trec_pager();
int init_trec_pager_text(), trec_pager_text(), close_trec_pager_text();
int init_trec_inter(), trec_inter(), close_trec_inter();
*/

int init_show_doc_part(), show_named_doc_part(), close_show_doc_part();
int init_show_part_vec(), show_part_vec(), close_show_part_vec();
int init_compare(), compare(), close_compare();
int init_segment(), segment(), close_segment();
int init_theme(), theme(), close_theme();
int init_path(), path(), close_path();

int init_summ_fixed(), summ_fixed(), close_summ_fixed();
int init_summ_best(), summ_best(), close_summ_best();
int init_summ_adhoc_fixed(), summ_adhoc_fixed(), close_summ_adhoc_fixed();
int init_summ_adhoc_best(), summ_adhoc_best(), close_summ_adhoc_best();


/* ------------------------------------------------------------- */
/*                    STANDARD MENUS                              */
/* ------------------------------------------------------------- */

static PROC_TAB proc_commands[] = {
    {"show_titles",    init_show_titles, inter_show_titles, close_show_titles},
    {"show_next", init_show_doc, show_next_doc, close_show_doc},
    {"show_named", init_show_doc, show_named_doc, close_show_doc},
    {"show_current", init_show_doc, show_current_doc, close_show_doc},
    {"show_high_next", init_show_high_doc, show_high_next_doc, close_show_high_doc},
    {"show_high_named", init_show_high_doc, show_high_named_doc, close_show_high_doc},
    {"show_high_current", init_show_high_doc, show_high_current_doc, close_show_high_doc},
    {"retrieve_more", init_run_vec, run_qvec, close_run_vec},
    {"run", init_run_vec, run_new, close_run_vec},
    {"feedback_list",  init_run_vec, run_feedback, close_run_vec},
    {"run_fb",  init_run_vec, run_with_feedback, close_run_vec},
    {"quit",     EMPTY, quit_inter, EMPTY},
    {"exit"     , EMPTY, exit_inter, EMPTY},
    {"save", init_inter_save_file, inter_save_file,close_inter_save_file},
    {"help", EMPTY, help_message, EMPTY},

    {"raw_doc", init_show_doc, set_raw_doc, close_show_doc},
    {"format_doc", init_show_doc, set_format_doc, close_show_doc},
    {"show_inv", init_show_inv, show_inv, close_show_inv}, 
    {"show_tr", init_show_raw, show_raw_tr, close_show_raw},
    {"show_location", init_show_raw, show_raw_textloc, close_show_raw},
    {"show_spec", init_show_raw, show_spec, close_show_raw}, 
    {"show_doc_vector",  init_show_vec, show_dvec, close_show_vec},
    {"run_doc",  init_run_vec, run_dvec, close_run_vec},
    {"sim_doc_doc", init_sim_vec, sim_dvec, close_sim_vec},
    {"match_doc_doc", init_match_vec, match_dvec, close_match_vec},
    {"show_query", EMPTY, show_query, EMPTY},
    {"show_q_vector", init_show_vec, show_qvec, close_show_vec},
    {"sim_q_doc", init_sim_vec, sim_qvec, close_sim_vec},
    {"match_q_doc", init_match_vec, match_qvec, close_match_vec},

    {"show_list", init_spec_list, show_spec_list, EMPTY},
    {"add_list", init_spec_list, add_spec_list, EMPTY},
    {"use_spec", init_spec_list, use_spec_list, EMPTY},
    {"set_query", init_set_query, set_query, close_set_query},
    {"show_rr", init_show_exp, show_exp_rr, close_show_exp},
    {"show_tr", init_show_exp, show_exp_tr, close_show_exp},
    {"show_q_eval", init_show_exp, show_exp_q_eval, close_show_exp},
    {"show_eval_list", init_show_exp, show_exp_eval, close_show_exp},
    {"show_rclprn_list", init_show_exp, show_exp_rclprn, close_show_exp},
    {"show_ind_rr_list", init_show_exp, show_exp_ind_rr, close_show_exp},
    {"show_ind_tr_list", init_show_exp, show_exp_ind_tr, close_show_exp},

    /* Following should be in liblocal?? */
    /*    {"Trec qid", init_trec_pager, trec_pager, close_trec_pager}, */
    //{"trec_pager", init_trec_pager, trec_pager, close_trec_pager},
    //{"trec_pager_text", init_trec_pager_text, trec_pager_text, close_trec_pager_text},
    {"show_parts", init_show_doc_part, show_named_doc_part, close_show_doc_part},
    {"show_parts_vec", init_show_part_vec, show_part_vec, close_show_part_vec},
    {"compare", init_compare, compare, close_compare},
    {"segment", init_segment, segment, close_segment},
    {"theme", init_theme, theme, close_theme},
    {"path", init_path, path, close_path}, 

    /* Summarization stuff */
    {"summ_fixed", init_summ_fixed, summ_fixed, close_summ_fixed},
    {"summ_best", init_summ_best, summ_best, close_summ_best},
    {"summ_adhoc_f", init_summ_adhoc_fixed, summ_adhoc_fixed, close_summ_adhoc_fixed},
    {"summ_adhoc_b", init_summ_adhoc_best, summ_adhoc_best, close_summ_adhoc_best},
};
static int num_proc_commands =
    sizeof (proc_commands) / sizeof (proc_commands[0]);


TAB_PROC_TAB proc_inter[] = {
    {"inter", 	TPT_PROC, NULL, proc_topinter, &num_proc_topinter},
    {"commands", TPT_PROC, NULL, proc_commands, &num_proc_commands},
    };
int num_proc_inter = sizeof (proc_inter) / sizeof (proc_inter[0]);
