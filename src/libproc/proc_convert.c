#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libproc/proc_convert.c,v 11.2 1993/02/03 16:53:40 smart Exp $";
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

extern int init_convert(), convert(), close_convert();
static PROC_TAB topproc_convert[] = {
    {"convert",  init_convert,       convert,    close_convert},
};
static int num_topproc_convert =
    sizeof(topproc_convert) / sizeof(topproc_convert[0]);


extern int init_vec_inv_obj(), vec_inv_obj(), close_vec_inv_obj();
extern int init_vec_aux_obj(), vec_aux_obj(), close_vec_aux_obj();
extern int init_vecwt_aux_obj(), vecwt_aux_obj(), close_vecwt_aux_obj();
extern int init_text_dict_obj(), text_dict_obj(), close_text_dict_obj();
extern int init_text_rr_obj(), text_rr_obj(), close_text_rr_obj();
extern int init_text_tr_obj(), text_tr_obj(), close_text_tr_obj();
extern int init_text_vec_obj(), text_vec_obj(), close_text_vec_obj();
extern int init_weight_doc(), weight_doc(), close_weight_doc();
extern int init_weight_query(), weight_query(), close_weight_query();
extern int init_tr_rel_obj(), tr_rel_obj(), close_tr_rel_obj();
extern int init_vec_vec_obj(), vec_vec_obj(), close_vec_vec_obj();
extern int init_inv_inv_obj(), inv_inv_obj(), close_inv_inv_obj();
extern int init_textloc_textloc_obj(), textloc_textloc_obj(), 
           close_textloc_textloc_obj();
extern int init_lang_model_wt_lm(), lang_model_wt_lm(), 
           close_lang_model_wt_lm();
extern int init_lang_model_wt_cf_lm(), lang_model_wt_cf_lm(), 
           close_lang_model_wt_cf_lm();
extern int init_lang_model_wt_cf_lm_nsim(), lang_model_wt_cf_lm_nsim(), 
           close_lang_model_wt_cf_lm_nsim();
extern int init_merge_vec_files(), merge_vec_files(), close_merge_vec_files();
extern int init_reduce_vec_lmscore(), reduce_vec_lmscore(), close_reduce_vec_lmscore();
extern int init_weight_exp_doc(), weight_exp_doc(), close_weight_exp_doc();
extern int init_weight_lda_doc(), weight_lda_doc(), close_weight_lda_doc();

static PROC_TAB proc_obj[] = {
    {"vec_inv",  init_vec_inv_obj,  vec_inv_obj,   close_vec_inv_obj},
    {"vec_aux",  init_vec_aux_obj,  vec_aux_obj,   close_vec_aux_obj},
    {"vecwt_aux",init_vecwt_aux_obj,vecwt_aux_obj, close_vecwt_aux_obj},
    {"text_dict",init_text_dict_obj,text_dict_obj, close_text_dict_obj},
    {"text_rr",  init_text_rr_obj,  text_rr_obj,   close_text_rr_obj},
    {"text_tr",  init_text_tr_obj,  text_tr_obj,   close_text_tr_obj},
    {"text_vec", init_text_vec_obj, text_vec_obj,  close_text_vec_obj},
    {"weight_doc", init_weight_doc, weight_doc,    close_weight_doc},
	/* For weighting a document with LDA (Liu, Croft) */
    {"weight_lda_doc", init_weight_lda_doc, weight_lda_doc,    close_weight_lda_doc},
    {"weight_query", init_weight_query, weight_query,  close_weight_query},
    {"tr_rel",   init_tr_rel_obj,   tr_rel_obj,    close_tr_rel_obj},
    {"vec_vec",  init_vec_vec_obj,  vec_vec_obj,   close_vec_vec_obj},
    {"inv_inv",  init_inv_inv_obj,  inv_inv_obj,   close_inv_inv_obj},
    {"textloc_textloc",  init_textloc_textloc_obj,  textloc_textloc_obj,
                close_textloc_textloc_obj},
	{ "weight_doc_lm", init_lang_model_wt_lm, lang_model_wt_lm, close_lang_model_wt_lm },
	{ "weight_doc_cf_lm", init_lang_model_wt_cf_lm, lang_model_wt_cf_lm, close_lang_model_wt_cf_lm },
	{ "weight_doc_cf_lm_nsim", init_lang_model_wt_cf_lm_nsim, lang_model_wt_cf_lm_nsim, close_lang_model_wt_cf_lm_nsim },
	{ "merge_vec_files", init_merge_vec_files, merge_vec_files, close_merge_vec_files},
	{ "reduce_vec_lmscore", init_reduce_vec_lmscore, reduce_vec_lmscore, close_reduce_vec_lmscore},
    {"weight_exp_doc", init_weight_exp_doc, weight_exp_doc,    close_weight_exp_doc},
    };
static int num_proc_obj = sizeof (proc_obj) / sizeof (proc_obj[0]);

extern int init_vec_inv(), vec_inv(), close_vec_inv();
extern int init_text_dict(), text_dict(), close_text_dict();
extern int init_contok_dict(), contok_dict(), close_contok_dict();
extern int init_vec_collstat(), vec_collstat(), close_vec_collstat();
extern int init_vec_collstat_inv(), vec_collstat_inv(), close_vec_collstat_inv();
extern int init_vec_invpos(), vec_invpos(), close_vec_invpos();	// for positional indexing
static PROC_TAB proc_tup[] = {
    {"vec_inv",          init_vec_inv,   vec_inv,        close_vec_inv},
    {"text_dict",        init_text_dict, text_dict,      close_text_dict},
    {"contok_dict",	init_contok_dict,contok_dict,	close_contok_dict},
    {"vec_collstat",	init_vec_collstat,vec_collstat,	close_vec_collstat},
    {"vec_collstat_inv",      init_vec_collstat_inv,vec_collstat_inv, close_vec_collstat_inv},
    {"vec_inv_pos",      init_vec_invpos, vec_invpos, close_vec_invpos},
    };
static int num_proc_tup = sizeof (proc_tup) / sizeof (proc_tup[0]);


extern int init_std_weight(), std_weight(), close_std_weight(); 
extern int init_separate_weight(), separate_weight(), close_separate_weight(); 
extern int init_weight_no(), weight_no(), close_weight_no(); 

// Expansion with decompounds
extern int init_vecexp_decomp(), vecexp_decomp(), close_vecexp_decomp();

static PROC_TAB proc_wt[] = {
    {"weight",		init_std_weight,std_weight,	close_std_weight}, 
    {"weight_separate",	init_separate_weight,separate_weight,	close_separate_weight}, 
    {"weight_no",	init_weight_no,	weight_no,	close_weight_no}, 
	{"weight_lm", init_lang_model_wt_lm, lang_model_wt_lm, close_lang_model_wt_lm },
	{"weight_cf_lm", init_lang_model_wt_cf_lm, lang_model_wt_cf_lm, close_lang_model_wt_cf_lm },
	{"weight_cf_lm_nsim", init_lang_model_wt_cf_lm_nsim, lang_model_wt_cf_lm_nsim, close_lang_model_wt_cf_lm_nsim },
    {"vecexp_decomp", init_vecexp_decomp, vecexp_decomp, close_vecexp_decomp},
};
static int num_proc_wt = sizeof (proc_wt) / sizeof (proc_wt[0]);

extern int init_weight_tri(), weight_tri(), close_weight_tri(); 
extern int init_weight_tfidf(), weight_tfidf(), close_weight_tfidf(); 
static PROC_TAB proc_wt_ctype[] = {
    {"weight_tri",init_weight_tri,	weight_tri,	close_weight_tri}, 
    {"weight_tfidf",init_weight_tfidf,	weight_tfidf,	close_weight_tfidf}, 
    {"no",		INIT_EMPTY,	EMPTY,		CLOSE_EMPTY},
    };
static int num_proc_wt_ctype = sizeof (proc_wt_ctype) /
         sizeof (proc_wt_ctype[0]);

int tfwt_binary(), tfwt_max(), tfwt_aug(), tfwt_square();
int tfwt_log(), tfwt_aug_log(), tfwt_log_log();
static PROC_TAB proc_wt_tf[] = {
    {"x",               EMPTY,         EMPTY,         EMPTY},
    {"n",               EMPTY,         EMPTY,         EMPTY},
    {"b",               EMPTY,         tfwt_binary,   EMPTY},
    {"m",               EMPTY,         tfwt_max,      EMPTY},
    {"a",               EMPTY,         tfwt_aug,      EMPTY},
    {"l",               EMPTY,         tfwt_log,      EMPTY},
};
static int num_proc_wt_tf = sizeof (proc_wt_tf) / sizeof (proc_wt_tf[0]);

int init_idfwt_idf(), idfwt_idf(), close_idfwt_idf();
int init_idfwt_s_idf(), idfwt_s_idf(), close_idfwt_s_idf();
static PROC_TAB proc_wt_idf[] = {
    {"x",               EMPTY,         EMPTY,         EMPTY},
    {"n",               EMPTY,         EMPTY,         EMPTY},
    {"s",        init_idfwt_s_idf,		idfwt_s_idf,     close_idfwt_s_idf},
    {"t",        init_idfwt_idf,		idfwt_idf,     close_idfwt_idf},
    {"i",        init_idfwt_idf,		idfwt_idf,     close_idfwt_idf},
};
static int num_proc_wt_idf = sizeof (proc_wt_idf) / sizeof (proc_wt_idf[0]);

int normwt_cos(), normwt_sum();
static PROC_TAB proc_wt_norm[] = {
    {"x",               EMPTY,         EMPTY,         EMPTY},
    {"n",               EMPTY,         EMPTY,         EMPTY},
    {"c",               EMPTY,         normwt_cos,    EMPTY},
    {"s",               EMPTY,         normwt_sum,    EMPTY},
};
static int num_proc_wt_norm = sizeof (proc_wt_norm) / sizeof (proc_wt_norm[0]);


extern int init_vecwt_unique(), vecwt_unique(), close_vecwt_unique(); 
extern int init_vecwt_cosine(), vecwt_cosine(), close_vecwt_cosine(); 
static PROC_TAB proc_vecwt_norm[] = {
    {"n", EMPTY,         EMPTY,         EMPTY},
    {"c", init_vecwt_cosine,    vecwt_cosine,	   close_vecwt_cosine}, 
    {"u", init_vecwt_unique, vecwt_unique, close_vecwt_unique},
};
static int num_proc_vecwt_norm = sizeof (proc_vecwt_norm) / sizeof (proc_vecwt_norm[0]);

extern int init_docexp_lm(), docexp_lm(), close_docexp_lm();	
extern int init_docexp_rlm(), docexp_rlm(), close_docexp_rlm();	
extern int init_docexp_trlm(), docexp_trlm(), close_docexp_trlm();
extern int init_docexp_trlm_coll(), docexp_trlm_coll(), close_docexp_trlm_coll();

static PROC_TAB proc_docexp[] = {
    {"lm", init_docexp_lm, docexp_lm, close_docexp_lm}, 
    {"rlm", init_docexp_rlm, docexp_rlm, close_docexp_rlm},
    {"trlm", init_docexp_trlm, docexp_trlm, close_docexp_trlm},
    {"trlm_coll", init_docexp_trlm_coll, docexp_trlm_coll, close_docexp_trlm_coll},
};
static int num_proc_docexp = sizeof (proc_docexp) / sizeof (proc_docexp[0]);

TAB_PROC_TAB proc_convert[] = {
    {"convert",  TPT_PROC, NULL, topproc_convert, &num_topproc_convert},
    {"obj",	TPT_PROC, NULL,	proc_obj,    	&num_proc_obj},
    {"tup",	TPT_PROC, NULL,	proc_tup,    	&num_proc_tup},
    {"weight",	TPT_PROC, NULL,	proc_wt, 	&num_proc_wt},
    {"wt_ctype",	TPT_PROC, NULL,	proc_wt_ctype, 	&num_proc_wt_ctype},
    {"wt_tf",	TPT_PROC, NULL,	proc_wt_tf, 	&num_proc_wt_tf},
    {"wt_idf",	TPT_PROC, NULL,	proc_wt_idf, 	&num_proc_wt_idf},
    {"wt_norm",	TPT_PROC, NULL,	proc_wt_norm, 	&num_proc_wt_norm},
    {"vecwt_norm",TPT_PROC, NULL,proc_vecwt_norm, 	&num_proc_vecwt_norm},
    {"doc_expansion", TPT_PROC, NULL, proc_docexp, 	&num_proc_docexp},
};
int num_proc_convert = sizeof (proc_convert) / sizeof (proc_convert[0]);
