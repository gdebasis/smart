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

static PROC_TAB topproc_ocr[] = {
    {"empty",            INIT_EMPTY,     EMPTY,          CLOSE_EMPTY}
};
static int num_topproc_ocr =
    sizeof(topproc_ocr) / sizeof(topproc_ocr[0]);

extern int init_vec_expvec_obj(), vec_expvec_obj(), close_vec_expvec_obj();
static PROC_TAB proc_obj[] = {
    {"vec_expvec",     init_vec_expvec_obj,vec_expvec_obj,  close_vec_expvec_obj},
    };
static int num_proc_obj = sizeof (proc_obj) / sizeof (proc_obj[0]);

extern int init_vec_exp_trie(), vec_exp_trie(), close_vec_exp_trie();
extern int init_vec_exp_trie_1(), vec_exp_trie_1(), close_vec_exp_trie_1();
extern int init_vec_exp_trie_2(), vec_exp_trie_2(), close_vec_exp_trie_2();
extern int init_vec_exp_trie_3(), vec_exp_trie_3(), close_vec_exp_trie_3();
extern int init_vec_exp_trie_4(), vec_exp_trie_4(), close_vec_exp_trie_4();
extern int init_vec_exp_trie_5(), vec_exp_trie_5(), close_vec_exp_trie_5();
extern int init_vec_exp_trie_6(), vec_exp_trie_6(), close_vec_exp_trie_6();
extern int init_vec_exp_trie_7(), vec_exp_trie_7(), close_vec_exp_trie_7();
static PROC_TAB proc_expand[] = {
    {"trie", init_vec_exp_trie, vec_exp_trie, close_vec_exp_trie},
    {"trie_1", init_vec_exp_trie_1, vec_exp_trie_1, close_vec_exp_trie_1},
    {"trie_2", init_vec_exp_trie_2, vec_exp_trie_2, close_vec_exp_trie_2},
    {"trie_3", init_vec_exp_trie_3, vec_exp_trie_3, close_vec_exp_trie_3},
    {"trie_4", init_vec_exp_trie_4, vec_exp_trie_4, close_vec_exp_trie_4},
    {"trie_5", init_vec_exp_trie_5, vec_exp_trie_5, close_vec_exp_trie_5},
    {"trie_6", init_vec_exp_trie_6, vec_exp_trie_6, close_vec_exp_trie_6},
    {"trie_7", init_vec_exp_trie_7, vec_exp_trie_7, close_vec_exp_trie_7},
    };
static int num_proc_expand = sizeof (proc_expand) / sizeof (proc_expand[0]);


extern int init_sim_ctype_inv_pos(), sim_ctype_inv_pos(), close_sim_ctype_inv_pos();
static PROC_TAB proc_ctype_coll[] = {
    {"ctype_inv_pos", init_sim_ctype_inv_pos, sim_ctype_inv_pos, close_sim_ctype_inv_pos},
    };
static int num_proc_ctype_coll = sizeof (proc_ctype_coll) / sizeof (proc_ctype_coll[0]);


TAB_PROC_TAB lproc_ocr[] = {
    {"ocr",  TPT_PROC, NULL, topproc_ocr, &num_topproc_ocr},
    {"obj",      TPT_PROC, NULL, proc_obj,       &num_proc_obj},
    {"expand",      TPT_PROC, NULL, proc_expand,       &num_proc_expand},
    {"ctype_coll",      TPT_PROC, NULL, proc_ctype_coll,       &num_proc_ctype_coll},
    };
int num_lproc_ocr = sizeof (lproc_ocr) / sizeof (lproc_ocr[0]);
