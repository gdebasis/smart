#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libproc/proc_convert.c,v 11.2 1993/02/03 16:52:22 smart Exp $";
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

static PROC_TAB topproc_convert[] = {
    {"empty",            INIT_EMPTY,     EMPTY,          CLOSE_EMPTY}
};
static int num_topproc_convert =
    sizeof(topproc_convert) / sizeof(topproc_convert[0]);

extern int init_q_qtc_obj(), q_qtc_obj(), close_q_qtc_obj();
extern int init_trec_tr_obj(), trec_tr_obj(), close_trec_tr_obj();
extern int init_tr_inex_obj(), tr_inex_obj(), close_tr_inex_obj();
extern int init_tr_elementlist(), tr_elementlist(), close_tr_elementlist();
extern int init_tr_wt_elementlist(), tr_wt_elementlist(), close_tr_wt_elementlist();
extern int init_tr_trec_obj(), tr_trec_obj(), close_tr_trec_obj();
extern int init_tr_prob_obj(), tr_prob_obj(), close_tr_prob_obj();
extern int init_rr_prob_obj(), rr_prob_obj(), close_rr_prob_obj();
extern int init_vecwt_vec_obj(), vecwt_vec_obj(), close_vecwt_vec_obj();
extern int init_trec_qrels_obj(), trec_qrels_obj(), close_trec_qrels_obj();
extern int init_inex_qrels_obj(), inex_qrels_obj(), close_inex_qrels_obj();
extern int init_trec_qrels_tr_obj(), trec_qrels_tr_obj(),
           close_trec_qrels_tr_obj();
extern int init_did_convec_obj(), did_convec_obj(), close_did_convec_obj();
/*extern int init_did_phrvec_obj(), init_qid_phrvec_obj(), id_phrvec_obj(), 
	   close_id_phrvec_obj();*/
extern int init_vec_covec_obj(), vec_covec_obj(), close_vec_covec_obj();
extern int init_text_synphr_obj(), text_synphr_obj(), close_text_synphr_obj();
extern int init_vec_filtervec(), vec_filtervec(), close_vec_filtervec();
extern int init_text_ltrie_obj(), text_ltrie_obj(), close_text_ltrie_obj();
extern int init_vec_corr_obj(), vec_corr_obj(), close_vec_corr_obj();
extern int init_text_boolvec_obj(), text_boolvec_obj(), close_text_boolvec_obj();
extern int init_text_tiledvec_obj(), text_tiledvec_obj(), close_text_tiledvec_obj();
extern int init_destem_obj(), destem_obj(), close_destem_obj();
extern int init_weight_fuzzy(), weight_fuzzy(), close_weight_fuzzy();
extern int init_text_inexfolsub(), text_inexfolsub(), close_text_inexfolsub();
extern int init_text_inex_fcs_to_ric(), text_inex_fcs_to_ric(), close_text_inex_fcs_to_ric();
extern int init_ws_vec(), ws_vec(), close_ws_vec();
extern int init_tr_tr_rel_rerank(), tr_tr_rel_rerank(), close_tr_tr_rel_rerank();
extern int init_tr_sbqe_obj(), tr_sbqe_obj(), close_tr_sbqe_obj();
extern int init_tr_sbqr_obj(), tr_sbqr_obj(), close_tr_sbqr_obj();
extern int init_local_text_vec_obj(), local_text_vec_obj(), close_local_text_vec_obj();
extern int init_tr_sbqr_incr_obj(), tr_sbqr_incr_obj(), close_tr_sbqr_incr_obj();
extern int init_tr_redqvec_obj(), tr_redqvec_obj(), close_tr_redqvec_obj();
extern int init_tr_lda_input(), tr_lda_input(), close_tr_lda_input();
extern int init_tr_tr_rlm(), tr_tr_rlm(), close_tr_tr_rlm();
extern int init_tr_tr_trlm(), tr_tr_trlm(), close_tr_tr_trlm();
extern int init_tr_tr_itrlm(), tr_tr_itrlm(), close_tr_tr_itrlm();
extern int init_tr_tr_trlm_inter(), tr_tr_trlm_inter(), close_tr_tr_trlm_inter();
extern int init_tr_tr_trlm_phrase(), tr_tr_trlm_phrase(), close_tr_tr_trlm_phrase();
extern int init_tr_tr_rlm_qexp(), tr_tr_rlm_qexp(), close_tr_tr_rlm_qexp();
extern int init_tr_tr_trlm_qexp(), tr_tr_trlm_qexp(), close_tr_tr_trlm_qexp();
extern int init_tr_tr_trlm_clir(), tr_tr_trlm_clir(), close_tr_tr_trlm_clir();
extern int init_tr_tr_trlm_clir_bilda(), tr_tr_trlm_clir_bilda(), close_tr_tr_trlm_clir_bilda();
extern int init_tr_tr_trlm_clir_dict(), tr_tr_trlm_clir_dict(), close_tr_tr_trlm_clir_dict();
extern int init_tr_tr_editdist(), tr_tr_editdist(), close_tr_tr_editdist();
extern int init_tr_inexqa_obj(), tr_inexqa_obj(), close_tr_inexqa_obj();

static PROC_TAB proc_obj[] = {
    {"q_qtc",        init_q_qtc_obj,   q_qtc_obj,    close_q_qtc_obj},
    {"vec_corr",     init_vec_corr_obj,   vec_corr_obj,    close_vec_corr_obj},
    {"trec_tr",      init_trec_tr_obj,   trec_tr_obj,    close_trec_tr_obj},
    {"tr_inex",      init_tr_inex_obj,   tr_inex_obj,    close_tr_inex_obj},
    {"tr_elts",      init_tr_elementlist, tr_elementlist, close_tr_elementlist},
    {"tr_wt_elts",      init_tr_wt_elementlist, tr_wt_elementlist, close_tr_wt_elementlist},
    {"tr_trec",        init_tr_trec_obj,   tr_trec_obj,    close_tr_trec_obj},
    {"tr_prob",        init_tr_prob_obj,   tr_prob_obj,    close_tr_prob_obj},
    {"rr_prob",        init_rr_prob_obj,   rr_prob_obj,    close_rr_prob_obj},
    {"vecwt_vec",      init_vecwt_vec_obj, vecwt_vec_obj,  close_vecwt_vec_obj},
    {"trec_qrels",     init_trec_qrels_obj, trec_qrels_obj,
                       close_trec_qrels_obj},
    {"inex_qrels",     init_inex_qrels_obj, inex_qrels_obj,
                       close_inex_qrels_obj},
    {"trec_qrels_tr",  init_trec_qrels_tr_obj,trec_qrels_tr_obj,
                       close_trec_qrels_tr_obj},
    {"did_convec",     init_did_convec_obj, did_convec_obj,
                       close_did_convec_obj},
/*    {"did_phr", init_did_phrvec_obj, id_phrvec_obj, close_id_phrvec_obj},
    {"qid_phr", init_qid_phrvec_obj, id_phrvec_obj, close_id_phrvec_obj},*/
    {"vec_covec",        init_vec_covec_obj,   vec_covec_obj,    close_vec_covec_obj},
    {"text_synphr",	init_text_synphr_obj,	text_synphr_obj,	close_text_synphr_obj},
    {"vec_fvec", init_vec_filtervec, vec_filtervec, close_vec_filtervec},
    {"text_ltrie",   init_text_ltrie_obj, text_ltrie_obj, close_text_ltrie_obj},
    {"text_boolvec",   init_text_boolvec_obj, text_boolvec_obj, close_text_boolvec_obj},
    {"text_tiledvec",  init_text_tiledvec_obj, text_tiledvec_obj, close_text_tiledvec_obj},
    {"text_inexfol",  init_text_inexfolsub, text_inexfolsub, close_text_inexfolsub},
    {"text_inex_fcs_to_ric",  init_text_inex_fcs_to_ric, text_inex_fcs_to_ric, close_text_inex_fcs_to_ric},
    {"text_vec",  init_local_text_vec_obj, local_text_vec_obj, close_local_text_vec_obj},
    {"destem",   init_destem_obj, destem_obj, close_destem_obj},
    {"ws_vec",   init_ws_vec, ws_vec, close_ws_vec},
    {"tr_tr_relrerank",   init_tr_tr_rel_rerank, tr_tr_rel_rerank, close_tr_tr_rel_rerank},
    {"tr_sbqe",   init_tr_sbqe_obj, tr_sbqe_obj, close_tr_sbqe_obj},
    {"tr_sbqr",   init_tr_sbqr_obj, tr_sbqr_obj, close_tr_sbqr_obj},
    {"tr_sbqr_incremental",   init_tr_sbqr_incr_obj, tr_sbqr_incr_obj, close_tr_sbqr_incr_obj},
    {"tr_term_redqvec",   init_tr_redqvec_obj, tr_redqvec_obj, close_tr_redqvec_obj},
    {"tr_lda_input",      init_tr_lda_input,   tr_lda_input,    close_tr_lda_input},
    {"tr_tr_rlm",         init_tr_tr_rlm,   tr_tr_rlm,    close_tr_tr_rlm},
    {"tr_tr_trlm",        init_tr_tr_trlm,   tr_tr_trlm,    close_tr_tr_trlm},
    {"tr_tr_itrlm",        init_tr_tr_itrlm,   tr_tr_itrlm,    close_tr_tr_itrlm},
    {"tr_tr_trlm_inter",        init_tr_tr_trlm_inter,   tr_tr_trlm_inter,    close_tr_tr_trlm_inter},
    {"tr_tr_trlm_phrase",        init_tr_tr_trlm_phrase,   tr_tr_trlm_phrase,    close_tr_tr_trlm_phrase},
    {"tr_tr_rlm_qexp",         init_tr_tr_rlm_qexp,   tr_tr_rlm_qexp,    close_tr_tr_rlm_qexp},
    {"tr_tr_trlm_qexp",        init_tr_tr_trlm_qexp,   tr_tr_trlm_qexp,    close_tr_tr_trlm_qexp},
    {"tr_tr_trlm_clir",   init_tr_tr_trlm_clir,   tr_tr_trlm_clir,    close_tr_tr_trlm_clir},
    {"tr_tr_trlm_clir_bilda",   init_tr_tr_trlm_clir_bilda,   tr_tr_trlm_clir_bilda,    close_tr_tr_trlm_clir_bilda},
    {"tr_tr_trlm_clir_dict",   init_tr_tr_trlm_clir_dict,   tr_tr_trlm_clir_dict,    close_tr_tr_trlm_clir_dict},
    {"tr_tr_editdist",   init_tr_tr_editdist,   tr_tr_editdist,    close_tr_tr_editdist},
    {"tr_sent",   init_tr_inexqa_obj,   tr_inexqa_obj,    close_tr_inexqa_obj},
};
static int num_proc_obj = sizeof (proc_obj) / sizeof (proc_obj[0]);

extern int init_contok_dict_noinfo(), contok_dict_noinfo(),
    close_contok_dict_noinfo();
extern int init_contok_beng(), contok_beng(), close_contok_beng();
extern int init_vec_inv(), vec_inv(), close_vec_inv();
static PROC_TAB proc_tup[] = {
    {"contok_dict_noinfo",     init_contok_dict_noinfo,   contok_dict_noinfo,
              close_contok_dict_noinfo},
    {"contok_beng", init_contok_beng, contok_beng, close_contok_beng},
    {"lvec_inv_tmp", init_vec_inv, vec_inv, close_vec_inv},
    };
static int num_proc_tup = sizeof (proc_tup) / sizeof (proc_tup[0]);


extern int init_weight_slope(), weight_slope(), close_weight_slope(); 
extern int init_weight_log(), weight_log(), close_weight_log(); 
extern int init_weight_len(), weight_len(), close_weight_len(); 
extern int local_init_lang_model_wt_lm(), local_lang_model_wt_lm(), local_close_lang_model_wt_lm(); 
static PROC_TAB proc_wt[] = {
    {"empty",            INIT_EMPTY,     EMPTY,          CLOSE_EMPTY},
    {"weight_slope", init_weight_slope, weight_slope, close_weight_slope}, 
    {"weight_log", init_weight_log, weight_log, close_weight_log}, 
    {"weight_len", init_weight_len, weight_len, close_weight_len}, 
    {"weight_fuzzy",   init_weight_fuzzy, weight_fuzzy, close_weight_fuzzy},
    {"weight_lm",  local_init_lang_model_wt_lm, local_lang_model_wt_lm, local_close_lang_model_wt_lm}
    };
static int num_proc_wt = sizeof (proc_wt) / sizeof (proc_wt[0]);

static PROC_TAB proc_wt_ctype[] = {
    {"empty",            INIT_EMPTY,     EMPTY,          CLOSE_EMPTY}
    };
static int num_proc_wt_ctype = sizeof (proc_wt_ctype) /
         sizeof (proc_wt_ctype[0]);

extern int tfwt_SQUARE(), tfwt_NONE(), tfwt_LOG(), tfwt_ALOG();
extern int tfwt_HYP(), tfwt_OKAPI(), tfwt_EX_HYP(), tfwt_AUG();
extern int tfwt_hyp(), tfwt_ex_hyp();
extern int init_tfwt_okapi(), tfwt_okapi(), close_tfwt_okapi();
extern int init_tfwt_inquery(), tfwt_inquery(), close_tfwt_inquery();
extern int init_tfwt_norm(), tfwt_norm(), close_tfwt_norm();
static PROC_TAB proc_wt_tf[] = {
    {"S",               EMPTY,         tfwt_SQUARE,   EMPTY},
    {"N",               EMPTY,         tfwt_NONE,     EMPTY},
    {"L",               EMPTY,         tfwt_LOG,      EMPTY},
    {"Z",               EMPTY,         tfwt_ALOG,     EMPTY},
    {"H",               EMPTY,         tfwt_HYP,      EMPTY},
    {"O",               EMPTY,         tfwt_OKAPI,    EMPTY},
    {"E",               EMPTY,         tfwt_EX_HYP,   EMPTY},
    {"h",               EMPTY,         tfwt_hyp,      EMPTY},
    {"e",               EMPTY,         tfwt_ex_hyp,   EMPTY},
    {"A",               EMPTY,         tfwt_AUG,      EMPTY},
    {"o", init_tfwt_okapi,   tfwt_okapi,   close_tfwt_okapi},
    {"i", init_tfwt_inquery, tfwt_inquery, close_tfwt_inquery},
    {"M", init_tfwt_norm, tfwt_norm, EMPTY},
};
static int num_proc_wt_tf = sizeof (proc_wt_tf) / sizeof (proc_wt_tf[0]);

int init_idfwt_IDF(), idfwt_IDF(), close_idfwt_IDF();
int init_idfwt_BIDF(), idfwt_BIDF(), close_idfwt_BIDF();
int init_idfwt_ictf(), idfwt_ictf(), close_idfwt_ictf();
int init_idfwt_normidf(), idfwt_normidf(), close_idfwt_normidf();
int idfwt_sqrtidf();
/* int init_idfwt_min(), idfwt_min(), close_idfwt_min(); */
static PROC_TAB proc_wt_idf[] = {
    {"T",		init_idfwt_IDF,	idfwt_IDF,	close_idfwt_IDF},
    {"B",		init_idfwt_BIDF,idfwt_BIDF,	close_idfwt_BIDF},
    {"C",		init_idfwt_ictf,idfwt_ictf,	close_idfwt_ictf},
    {"N",		init_idfwt_normidf, idfwt_normidf, close_idfwt_normidf},
    {"S",		init_con_cw_idf, idfwt_sqrtidf, close_con_cw_idf},
/*  {"M",		init_idfwt_min, idfwt_min,	close_idfwt_min}, */
};
static int num_proc_wt_idf = sizeof (proc_wt_idf) / sizeof (proc_wt_idf[0]);

int init_normwt_bytelength(), normwt_bytelength(), close_normwt_bytelength();
static PROC_TAB proc_wt_norm[] = {
    {"b", init_normwt_bytelength, normwt_bytelength,  close_normwt_bytelength},
};
static int num_proc_wt_norm = sizeof (proc_wt_norm) / sizeof (proc_wt_norm[0]);

int init_local_vecwt_unique(), local_vecwt_unique(), close_local_vecwt_unique();
static PROC_TAB proc_vecwt_norm[] = {
    {"U", init_local_vecwt_unique, local_vecwt_unique, close_local_vecwt_unique},
};
static int num_proc_vecwt_norm = sizeof (proc_vecwt_norm) / sizeof (proc_vecwt_norm[0]);

extern int init_tr_tr_interleave(), tr_tr_interleave(), close_tr_tr_interleave();
/*
extern int init_tr_tr_rerank(), tr_tr_rerank(), close_tr_tr_rerank();
extern int init_rerank_pnorm(), rerank_pnorm(), close_rerank_pnorm();
*/
extern int init_rerank_coocc_lidf(), rerank_coocc_lidf(), close_rerank_coocc_lidf();
/*
extern int init_rerank_nnsim(), rerank_nnsim(), close_rerank_nnsim();
extern int init_rerank_cluster_nosel(), rerank_cluster_nosel(), 
           close_rerank_cluster_nosel();
extern int init_rerank_cluster_selall(), rerank_cluster_selall(), 
           close_rerank_cluster_selall();
extern int init_rerank_cluster_retro(), rerank_cluster_retro(), 
           close_rerank_cluster_retro();
*/
static PROC_TAB proc_rerank[] = {
/*
    {"rewt",		init_tr_tr_rerank,	tr_tr_rerank,	close_tr_tr_rerank},
    {"pnorm",		init_rerank_pnorm,	rerank_pnorm,	close_rerank_pnorm},
*/
    {"coocc_lidf", init_rerank_coocc_lidf, rerank_coocc_lidf, close_rerank_coocc_lidf},
/*
    {"nnsim",		init_rerank_nnsim,	rerank_nnsim,	close_rerank_nnsim},
    {"cluster_nosel",	init_rerank_cluster_nosel, rerank_cluster_nosel, close_rerank_cluster_nosel},
    {"cluster_selall",	init_rerank_cluster_selall, rerank_cluster_selall, close_rerank_cluster_selall},
    {"cluster_retro",	init_rerank_cluster_retro, rerank_cluster_retro, close_rerank_cluster_retro},
*/
    {"interleave",		init_tr_tr_interleave,	tr_tr_interleave,	close_tr_tr_interleave},
    {"none", EMPTY, EMPTY, EMPTY}
};

static int num_proc_rerank = sizeof (proc_rerank) / sizeof (proc_rerank[0]);

TAB_PROC_TAB lproc_convert[] = {
    {"convert",  TPT_PROC, NULL, topproc_convert, &num_topproc_convert},
    {"obj",      TPT_PROC, NULL, proc_obj,       &num_proc_obj},
    {"tup",      TPT_PROC, NULL, proc_tup,       &num_proc_tup},
    {"weight",   TPT_PROC, NULL, proc_wt,        &num_proc_wt},
    {"wt_ctype", TPT_PROC, NULL, proc_wt_ctype,  &num_proc_wt_ctype},
    {"wt_tf",    TPT_PROC, NULL, proc_wt_tf,     &num_proc_wt_tf},
    {"wt_idf",   TPT_PROC, NULL, proc_wt_idf,    &num_proc_wt_idf},
    {"wt_norm",  TPT_PROC, NULL, proc_wt_norm,   &num_proc_wt_norm},
    {"vecwt_norm",  TPT_PROC, NULL, proc_vecwt_norm,   &num_proc_vecwt_norm},
    {"rerank",  TPT_PROC, NULL, proc_rerank,   &num_proc_rerank},
    };
int num_lproc_convert = sizeof (lproc_convert) / sizeof (lproc_convert[0]);
