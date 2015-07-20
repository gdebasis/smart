#ifndef FUNCTIONSH
#define FUNCTIONSH
/*        $Header: /home/smart/release/src/h/functions.h,v 11.2 1993/02/03 16:47:29 smart Exp $ */
/* Declarations of major functions within SMART, and in libraries */

#include "sysfunc.h"

/************************* SMART procedures ***********************/
#include "param.h"
#include "vector.h"
#include "rel_header.h"

/* SMART Global Variables */
extern long global_start;
extern long global_end;

/* lib_file functions: */
int create_array(), open_array(), seek_array(), read_array(),
        write_array(), close_array(), destroy_array(), rename_array();
int create_vector(), open_vector(), seek_vector(), read_vector(),
        write_vector(), close_vector(), destroy_vector(), rename_vector();
/* int create_dict(), open_dict(), seek_dict(), read_dict(),
        write_dict(), close_dict(), destroy_dict(), rename_dict(); */
int create_inv(), open_inv(), seek_inv(), read_inv(),
        write_inv(), close_inv(), destroy_inv(), rename_inv();
int create_tr_vec(), open_tr_vec(), seek_tr_vec(), read_tr_vec(),
        write_tr_vec(), close_tr_vec(), destroy_tr_vec(), rename_tr_vec();
int create_rr_vec(), open_rr_vec(), seek_rr_vec(), read_rr_vec(),
        write_rr_vec(), close_rr_vec(), destroy_rr_vec(), rename_rr_vec();
int create_textloc(), open_textloc(), seek_textloc(), read_textloc(),
        write_textloc(), close_textloc(), destroy_textloc(), rename_textloc();
int create_hier(), open_hier(), seek_hier(), read_hier(),
        write_hier(), close_hier(), destroy_hier(), rename_hier();
int create_graph(), open_graph(), seek_graph(), read_graph(),
        write_graph(), close_graph(), destroy_graph(), rename_graph();
int create_simp_inv(), open_simp_inv(), seek_simp_inv(), read_simp_inv(),
        write_simp_inv(), close_simp_inv(), destroy_simp_inv(),
        rename_simp_inv();
int create_dir_array(), open_dir_array(), seek_dir_array(), read_dir_array(),
        write_dir_array(), close_dir_array(), destroy_dir_array(),
        rename_dir_array();
int create_fdbkstats(), open_fdbkstats(), seek_fdbkstats(), read_fdbkstats(),
        write_fdbkstats(), close_fdbkstats(), destroy_fdbkstats(),
        rename_fdbkstats();
int create_direct(), open_direct(), seek_direct(), read_direct(),
        write_direct(), close_direct(), destroy_direct(), rename_direct();


/* lib_general functions: */
char *prepend_db();
REL_HEADER *get_rel_header();
int prepare_backup(), make_backup(), backup(), copy_fd(), copy();
int add_buf(), add_buf_string(), text_textline(), 
    text_long_array(), free_long_array();
int unix_page(), unix_edit_file(), unix_inout_command(), unix_file_exists();
int save_vec(), copy_vec(), free_vec(), save_vec_list(), free_vec_list();
int lookup_spec_docdesc(), lookup_spec_prefix();
int get_spec(), mod_spec(), lookup_spec(), copy_spec(), get_spec_list();
int getspec_string(), getspec_intstring(), getspec_float(), 
    getspec_double(), getspec_long(), getspec_bool(),
    getspec_filemode(), getspec_dbfile(), getspec_int();
CON_WT* searchCon(CON_WT* keyconwt, CON_WT* conwt, long num_conwt);

#ifdef ACCOUNTING
void init_accounting(), report_accounting();
#endif  /* ACCOUNTING */

void print_error(), print_error_buf();


/* libindexing functions: */
int check_sentence(), check_paragraph();
int init_index_doc_coll(), index_doc_coll(), close_index_doc_coll();
int init_index_query_coll(), index_query_coll(), close_index_query_coll();
int init_triestem(), triestem(), close_triestem();

/* Low level (normally to buffer) print procedures */
void print_accounting(), print_buf(),
     print_consect(), print_dict(), 
     print_fdbk_info(), print_float(), print_full_results(),
     print_inv(), print_long(), print_rel_header(), print_rr_vec(),
     print_sm_condoc(), print_sm_display(), 
     print_smeval(), print_spec(), print_string(),
     print_textloc(), print_tokendoc(), print_tokensect(), print_top_results(),
     print_tr_vec(), print_vector(), print_vec_list();
void print_int_textdoc(), print_int_textdoc_nohead(), print_int_textloc();
void print_ind_rr(), print_ind_tr(), print_rr_eval(), print_tr_eval(),
     print_rclprn();

/* Hierarchy print procedures */
int init_print_vec_dict(), print_vec_dict(), close_print_vec_dict();

/* libconvert functions : */
int init_did_vec(), did_vec(), close_did_vec();
int init_qid_vec(), qid_vec(), close_qid_vec();
/* int init_pp_veclist(), pp_veclist(), close_pp_veclist(); */
/* int init_pp_pp_parts(), pp_pp_parts(), close_pp_pp_parts(); */
int init_std_weight(), std_weight(), close_std_weight();
int init_sectid_num(), sectid_num(), close_sectid_num();
int init_text_dict_obj(), text_dict_obj(), close_text_dict_obj();
int init_text_rr_obj(), text_rr_obj(), close_text_rr_obj();
int init_con_inv(), con_inv(), close_con_inv();
int init_con_cw_idf(), con_cw_idf(), close_con_cw_idf();
/* int init_con_cw_pidf(), con_cw_pidf(), close_con_cw_pidf(); */
int init_con_cw_idf(), con_cw_idf(), close_con_cw_idf();
int init_con_cw_cf(), con_cw_cf(), close_con_cw_cf();
int init_con_cw_ictf(), con_cw_ictf(), close_con_cw_ictf();
int init_con_invidf(), con_invidf(), close_con_invidf();
/* int init_con_invpidf(), con_invpidf(), close_con_invpidf(); */
int init_con_invfreq(), con_invfreq(), close_con_invfreq();
int init_ret_tr(), ret_tr(), close_ret_tr();

//+++Debasis
int init_lang_model_wt_lm() ;
int lang_model_wt_lm() ;
int close_lang_model_wt_lm() ;
//---Debasis

int init_rrvec_smeval(), rrvec_smeval(), close_rrvec_smeval();
int init_trvec_smeval(), trvec_smeval(), close_trvec_smeval();
int init_tr_eval(), tr_eval(), close_tr_eval();
int init_rr_eval(), rr_eval(), close_rr_eval();

int res_tr();
int normwt_cos();

int init_wt_fdbk_lm(), wt_fdbk_lm(), close_wt_fdbk_lm();
int init_form_query_lm(), form_query_lm(), close_form_query_lm();

#define FREE(p) if (p) { free(p); p = NULL; }
#define twice(x) (x<<1)
#define half(x) (x>>1)

#endif /* FUNCTIONSH */

