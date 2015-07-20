#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/src/liblocal/libproc/proc_fdbk.c,v 11.0 92/07/14 11:10:17 smart Exp Locker: smart $";
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


extern int init_local_exp_reldoc(), local_exp_reldoc(), close_local_exp_reldoc();
extern int init_local_exp_reldoc_lm(), local_exp_reldoc_lm(), close_local_exp_reldoc_lm();
extern int init_local_exp_reldoc_rsv(), local_exp_reldoc_rsv(), close_local_exp_reldoc_rsv();
extern int init_local_exp_const(), local_exp_const(), close_local_exp_const();
extern int init_local_exp_const_lm(), local_exp_const_lm(), close_local_exp_const_lm();
extern int init_local_exp_const_rsv(), local_exp_const_rsv(), close_local_exp_const_rsv();
extern int init_exp_nonrand(), exp_nonrand(), close_exp_nonrand();
extern int init_exp_nonrand_co(), exp_nonrand_co(), close_exp_nonrand_co();
extern int init_exp_nonrand_prox(), exp_nonrand_prox(), close_exp_nonrand_prox();
extern int init_exp_stats_all(), exp_stats_all(), close_exp_stats_all();
extern int init_exp_cluster(), exp_cluster(), close_exp_cluster();

static PROC_TAB proc_exp[] = {
    {"exp_rel_doc", init_local_exp_reldoc, local_exp_reldoc, close_local_exp_reldoc},
    {"exp_const", init_local_exp_const,   local_exp_const,    close_local_exp_const},
    {"exp_rel_doc_lm", init_local_exp_reldoc_lm, local_exp_reldoc_lm, close_local_exp_reldoc_lm},
    {"exp_const_lm", init_local_exp_const_lm,   local_exp_const_lm,    close_local_exp_const_lm},
    {"exp_rel_doc_rsv", init_local_exp_reldoc_rsv, local_exp_reldoc_rsv, close_local_exp_reldoc_rsv},
    {"exp_const_rsv", init_local_exp_const_rsv,   local_exp_const_rsv,    close_local_exp_const_rsv},
    {"exp_nonrand", init_exp_nonrand, exp_nonrand, close_exp_nonrand},
    {"exp_nonrand_co", init_exp_nonrand_co, exp_nonrand_co, close_exp_nonrand_co},
    {"exp_nonrand_prox", init_exp_nonrand_prox, exp_nonrand_prox, close_exp_nonrand_prox},
    {"exp_stats_all", init_exp_stats_all, exp_stats_all, close_exp_stats_all},
    {"exp_cluster", init_exp_cluster, exp_cluster, close_exp_cluster},
};
static int num_proc_exp = sizeof(proc_exp) / sizeof(proc_exp[0]);


extern int init_exp_coocc(), exp_coocc(), close_exp_coocc();
extern int init_exp_corr(), exp_corr(), close_exp_corr();
extern int init_exp_corr_notok(), exp_corr_notok(), close_exp_corr_notok();
extern int init_exp_corr_r2r(), exp_corr_r2r(), close_exp_corr_r2r();
extern int init_exp_coocc_r2r(), exp_coocc_r2r(), close_exp_coocc_r2r();
static PROC_TAB proc_exp_coocc[] = {
    {"exp_corr_r2r", init_exp_corr_r2r, exp_corr_r2r, close_exp_corr_r2r},
    {"exp_corr", init_exp_corr, exp_corr, close_exp_corr},
    {"exp_corr_nt", init_exp_corr_notok, exp_corr_notok, close_exp_corr_notok},
    {"exp_coocc", init_exp_coocc, exp_coocc, close_exp_coocc},
    {"exp_coocc_r2r", init_exp_coocc_r2r, exp_coocc_r2r, close_exp_coocc_r2r},
    {"empty",      EMPTY,         EMPTY,       EMPTY},
};
static int num_proc_exp_coocc = sizeof(proc_exp_coocc) / sizeof(proc_exp_coocc[0]);


extern int init_local_occ_inv_stats(), local_occ_inv_stats(), close_local_occ_inv_stats();
extern int init_local_occ_inv(), local_occ_inv(), close_local_occ_inv();
extern int init_occ_stats_stats(), occ_stats_stats(), close_occ_stats_stats();

static PROC_TAB proc_occ[] = {
    {"inv_stats", init_local_occ_inv_stats, local_occ_inv_stats, close_local_occ_inv_stats},
    {"inv", init_local_occ_inv, local_occ_inv, close_local_occ_inv},
    {"occ_stats_stats", init_occ_stats_stats, occ_stats_stats, close_occ_stats_stats}
};
static int num_proc_occ = sizeof(proc_occ) / sizeof(proc_occ[0]);

extern int init_wt_fdbk_sel(), wt_fdbk_sel(), close_wt_fdbk_sel();
extern int init_wt_fdbk_sel_q(), wt_fdbk_sel_q(), close_wt_fdbk_sel_q();
extern int init_wt_fdbk_sel_q_all(), wt_fdbk_sel_q_all(), close_wt_fdbk_sel_q_all();
extern int init_wt_fdbk_sel_qprox(), wt_fdbk_sel_qprox(), close_wt_fdbk_sel_qprox();
extern int init_wt_fdbk_sel_corr(), wt_fdbk_sel_corr(), close_wt_fdbk_sel_corr();
extern int init_wt_fdbk_sel_cWITHrel(), wt_fdbk_sel_cWITHrel(), close_wt_fdbk_sel_cWITHrel();
extern int init_wt_fdbk_sel_corr_adhoc(), wt_fdbk_sel_corr_adhoc(), close_wt_fdbk_sel_corr_adhoc();
extern int init_wtfdbk_sel_q_clstridf(), wtfdbk_sel_q_clstridf(), close_wtfdbk_sel_q_clstridf();
extern int init_wtfdbk_sel_q_all_clstridf(), wtfdbk_sel_q_all_clstridf(), close_wtfdbk_sel_q_all_clstridf();
extern int init_wtfdbk_sel_q_all_clstrpidf(), wtfdbk_sel_q_all_clstrpidf(), close_wtfdbk_sel_q_all_clstrpidf();
extern int init_wtfdbk_sel_q_all_clstrretro(), wtfdbk_sel_q_all_clstrretro(), close_wtfdbk_sel_q_all_clstrretro();
static PROC_TAB proc_weight[] = {
    {"fdbk_sel", init_wt_fdbk_sel, wt_fdbk_sel, close_wt_fdbk_sel},
    {"fdbk_sel_q", init_wt_fdbk_sel_q, wt_fdbk_sel_q, close_wt_fdbk_sel_q},
    {"fdbk_sel_q_all", init_wt_fdbk_sel_q_all, wt_fdbk_sel_q_all, close_wt_fdbk_sel_q_all},
    {"fdbk_sel_qprox", init_wt_fdbk_sel_qprox, wt_fdbk_sel_qprox, close_wt_fdbk_sel_qprox},
    {"fdbk_sel_corr", init_wt_fdbk_sel_corr, wt_fdbk_sel_corr, close_wt_fdbk_sel_corr},
    {"fdbk_sel_corr_adhoc", init_wt_fdbk_sel_corr_adhoc, wt_fdbk_sel_corr_adhoc, close_wt_fdbk_sel_corr_adhoc},
    {"fdbk_sel_cWITHrel", init_wt_fdbk_sel_cWITHrel, wt_fdbk_sel_cWITHrel, close_wt_fdbk_sel_cWITHrel},
    {"fdbk_sel_q_clstridf", init_wtfdbk_sel_q_clstridf, wtfdbk_sel_q_clstridf, close_wtfdbk_sel_q_clstridf},
    {"fdbk_sel_q_all_clstridf", init_wtfdbk_sel_q_all_clstridf, wtfdbk_sel_q_all_clstridf, close_wtfdbk_sel_q_all_clstridf},
    {"fdbk_sel_q_all_clstrpidf", init_wtfdbk_sel_q_all_clstrpidf, wtfdbk_sel_q_all_clstrpidf, close_wtfdbk_sel_q_all_clstrpidf},
    {"fdbk_sel_q_all_clstrretro", init_wtfdbk_sel_q_all_clstrretro, wtfdbk_sel_q_all_clstrretro, close_wtfdbk_sel_q_all_clstrretro},
};
static int num_proc_weight = sizeof(proc_weight) / sizeof(proc_weight[0]);

extern int init_form_best_query(), form_best_query(), close_form_best_query();

static PROC_TAB proc_form[] = {
    {"form_best_query", init_form_best_query, form_best_query, close_form_best_query},
    {"empty",      EMPTY,         EMPTY,       EMPTY},
};
static int num_proc_form = sizeof(proc_form) / sizeof(proc_form[0]);


extern int init_best_terms_ret(), best_terms_ret(), close_best_terms_ret();
extern int init_best_ret_rd(), best_ret_rd(), close_best_ret_rd();
extern int init_best_ret_inst(), best_ret_inst(), close_best_ret_inst();
extern int init_best_ret_inst_fast(), best_ret_inst_fast(), close_best_ret_inst_fast();
extern int init_best_ret_inst_coocc(), best_ret_inst_coocc(), close_best_ret_inst_coocc();
static PROC_TAB proc_best[] = {
    {"ret_rd", init_best_ret_rd,   best_ret_rd,    close_best_ret_rd},
    {"ret_inst", init_best_ret_inst,   best_ret_inst,    close_best_ret_inst},
    {"ret_inst_fast",init_best_ret_inst_fast,best_ret_inst_fast,close_best_ret_inst_fast},
    {"ret_inst_coocc",init_best_ret_inst_coocc,best_ret_inst_coocc,close_best_ret_inst_coocc},
    {"best_terms_ret", init_best_terms_ret,   best_terms_ret,    close_best_terms_ret},
    {"empty",      EMPTY,         EMPTY,       EMPTY},
};
static int num_proc_best = sizeof(proc_best) / sizeof(proc_best[0]);

TAB_PROC_TAB lproc_fdbk[] = {
    {"expand",     TPT_PROC,   NULL, proc_exp,     	&num_proc_exp},
    {"occ_info",   TPT_PROC,   NULL, proc_occ,     	&num_proc_occ},
    {"weight",     TPT_PROC,   NULL, proc_weight,       &num_proc_weight},
    {"form",       TPT_PROC,   NULL, proc_form,         &num_proc_form},
    {"best_terms", TPT_PROC,   NULL, proc_best,         &num_proc_best},
    {"exp_coocc",  TPT_PROC,   NULL, proc_exp_coocc,   	&num_proc_exp_coocc},
};
int num_lproc_fdbk = sizeof (lproc_fdbk) / sizeof (lproc_fdbk[0]);
