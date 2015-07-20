#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/./src/libproc/proc_fdbk.c,v 11.0 92/07/14 11:10:17 smart Exp Locker: smart $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "proc.h"

extern int init_feedback(), feedback(), close_feedback();
extern int init_feedback_obj(), feedback_obj(), close_feedback_obj();
static PROC_TAB topproc_feedback[] = {
    {"feedback", init_feedback,   feedback,    close_feedback},
    {"feedback_obj", init_feedback_obj,   feedback_obj,    close_feedback_obj},
    {"empty",      EMPTY,         EMPTY,       EMPTY},
};
static int num_topproc_feedback =
    sizeof(topproc_feedback) / sizeof(topproc_feedback[0]);


extern int init_exp_rel_doc(), exp_rel_doc(), close_exp_rel_doc();
extern int init_exp_const(), exp_const(), close_exp_const();
extern int init_exp_const_stats(), exp_const_stats(), close_exp_const_stats();
extern int init_exp_const_stats_prob(), exp_const_stats_prob(), close_exp_const_stats_prob();
extern int init_exp_percent_stats(), exp_percent_stats(), close_exp_percent_stats();
static PROC_TAB proc_expand[] = {
    {"exp_rel_doc", init_exp_rel_doc,   exp_rel_doc,    close_exp_rel_doc},
    {"exp_const", init_exp_const,   exp_const,    close_exp_const},
    {"exp_const_stats_prob", init_exp_const_stats_prob,   exp_const_stats_prob,    close_exp_const_stats_prob},
    {"exp_const_stats", init_exp_const_stats,   exp_const_stats,    close_exp_const_stats},
    {"exp_percent_stats", init_exp_percent_stats,   exp_percent_stats,    close_exp_percent_stats},
    {"empty",      EMPTY,         EMPTY,       EMPTY},
};
static int num_proc_expand =
    sizeof(proc_expand) / sizeof(proc_expand[0]);

extern int init_occ_inv(), occ_inv(), close_occ_inv();
extern int init_occ_inv_all(), occ_inv_all(), close_occ_inv_all();
extern int init_occ_inv_stats(), occ_inv_stats(), close_occ_inv_stats();
extern int init_occ_inv_all_stats(), occ_inv_all_stats(), close_occ_inv_all_stats();
extern int init_occ_collstats_all_stats(), occ_collstats_all_stats(), close_occ_collstats_all_stats();
extern int init_occ_ide(), occ_ide(), close_occ_ide();
extern int init_occ_rocchio(), occ_rocchio(), close_occ_rocchio();
extern int init_occ_rocchio_stats(), occ_rocchio_stats(), close_occ_rocchio_stats();
static PROC_TAB proc_occ_info[] = {
    {"inv",      init_occ_inv,   occ_inv,    close_occ_inv},
    {"inv_all",  init_occ_inv_all,   occ_inv_all,    close_occ_inv_all},
    {"inv_stats",  init_occ_inv_stats,   occ_inv_stats,    close_occ_inv_stats},
    {"inv_all_stats",  init_occ_inv_all_stats,   occ_inv_all_stats,    close_occ_inv_all_stats},
    {"collstats_all_stats",  init_occ_collstats_all_stats,   occ_collstats_all_stats,    close_occ_collstats_all_stats},
    {"ide",      init_occ_ide,   occ_ide,    close_occ_ide},
    {"rocchio",      init_occ_rocchio,   occ_rocchio,    close_occ_rocchio},
    {"rocchio_stats", init_occ_rocchio_stats, occ_rocchio_stats, close_occ_rocchio_stats},
    {"empty",      EMPTY,         EMPTY,       EMPTY},
};
static int num_proc_occ_info =
    sizeof(proc_occ_info) / sizeof(proc_occ_info[0]);

extern int init_wt_fdbk(), wt_fdbk(), close_wt_fdbk();
extern int init_wt_fdbk_all(), wt_fdbk_all(), close_wt_fdbk_all();
extern int init_wt_ide(), wt_ide(), close_wt_ide();
extern int init_wt_prc_nclass(), wt_prc_nclass(), close_wt_prc_nclass();
extern int init_wt_prc_rev(), wt_prc_rev(), close_wt_prc_rev();
extern int init_wt_fdbk_lm(), wt_fdbk_lm(), close_wt_fdbk_lm();
extern int init_wt_fdbk_lm_em(), wt_fdbk_lm_em(), close_wt_fdbk_lm_em();

static PROC_TAB proc_weight[] = {
    {"fdbk",       init_wt_fdbk,  wt_fdbk,   close_wt_fdbk},
    {"fdbk_all",       init_wt_fdbk_all,  wt_fdbk_all,   close_wt_fdbk_all},
    {"roccio",     init_wt_fdbk,  wt_fdbk,   close_wt_fdbk},
    {"roccio_all",     init_wt_fdbk_all,  wt_fdbk_all,   close_wt_fdbk_all},
    {"ide",        init_wt_ide,   wt_ide,    close_wt_ide},
    {"prc_nclass",  init_wt_prc_nclass,   wt_prc_nclass,    close_wt_prc_nclass},
    {"prc_rev",  init_wt_prc_rev,   wt_prc_rev,    close_wt_prc_rev},
//+++Debasis
    {"fdbk_lm",  init_wt_fdbk_lm,   wt_fdbk_lm,    close_wt_fdbk_lm},
    {"fdbk_lm_em",  init_wt_fdbk_lm_em,   wt_fdbk_lm_em,    close_wt_fdbk_lm_em},
//---Debasis
    {"empty",      EMPTY,         EMPTY,       EMPTY},
};
static int num_proc_weight =
    sizeof(proc_weight) / sizeof(proc_weight[0]);

extern int init_form_query(), form_query(), close_form_query();
extern int init_form_wtquery(), form_wtquery(), close_form_wtquery();
extern int init_form_unnorm_query(), form_unnorm_query(), close_form_unnorm_query();
extern int init_form_query_lm(), form_query_lm(), close_form_query_lm();

static PROC_TAB proc_form[] = {
    {"form_query", init_form_query,   form_query,    close_form_query},
//+++Debasis
    {"form_query_lm", init_form_query_lm,   form_query_lm,    close_form_query_lm},
//---Debasis
    {"form_wtquery", init_form_wtquery,   form_wtquery,    close_form_wtquery},
    {"form_unnorm_query", init_form_unnorm_query,   form_unnorm_query,    close_form_unnorm_query},
    {"empty",      EMPTY,         EMPTY,       EMPTY},
};
static int num_proc_form =
    sizeof(proc_form) / sizeof(proc_form[0]);


TAB_PROC_TAB proc_feedback[] = {
    {"feedback",   TPT_PROC,   NULL, topproc_feedback,  &num_topproc_feedback},
    {"expand",     TPT_PROC,   NULL, proc_expand,       &num_proc_expand},
    {"occ_info",   TPT_PROC,   NULL, proc_occ_info,     &num_proc_occ_info},
    {"weight",     TPT_PROC,   NULL, proc_weight,       &num_proc_weight},
    {"form",       TPT_PROC,   NULL, proc_form,         &num_proc_form},
    };

int num_proc_feedback = sizeof (proc_feedback) / sizeof (proc_feedback[0]);




