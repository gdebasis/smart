#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libproc/proc_print.c,v 11.0 1992/07/21 18:23:08 chrisb Exp $";
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

extern int init_print(), print(), close_print();
extern int init_print_obj_querytext();
static PROC_TAB topproc_print[] = {
    {"print",  init_print,     print,     close_print},
};
static int num_topproc_print =
    sizeof(topproc_print) / sizeof(topproc_print[0]);

extern int init_print_obj_doctext(), print_obj_text(), close_print_obj_text();
extern int init_print_obj_querytext();
extern int init_print_obj_vec_dict(),
            print_obj_vec_dict(), close_print_obj_vec_dict();
extern int init_print_obj_vec(), print_obj_vec(), close_print_obj_vec();
extern int init_print_obj_tr_vec(),print_obj_tr_vec(),close_print_obj_tr_vec();
extern int init_print_obj_wiki(),print_obj_wiki(),close_print_obj_wiki();
extern int init_print_obj_eval(), print_obj_eval(),
            close_print_obj_eval();
extern int init_print_obj_qq_eval(), print_obj_qq_eval(),
            close_print_obj_qq_eval();
extern int init_print_obj_ref_eval(), print_obj_ref_eval(),
            close_print_obj_ref_eval();
extern int init_print_obj_textloc(), print_obj_textloc(), 
            close_print_obj_textloc();
extern int init_print_obj_rr_vec(),print_obj_rr_vec(),close_print_obj_rr_vec();
extern int init_print_obj_inv(), print_obj_inv(), close_print_obj_inv();
extern int init_print_obj_invpos(), print_obj_invpos(), close_print_obj_invpos();
extern int init_print_obj_dict(), print_obj_dict(), close_print_obj_dict();
extern int init_print_obj_proc(), print_obj_proc(), close_print_obj_proc();
extern int init_print_obj_rel_header(), print_obj_rel_header(), 
            close_print_obj_rel_header();
extern int init_print_obj_fdbk_stats(), print_obj_fdbk_stats(), 
            close_print_obj_fdbk_stats();
extern int init_eval_clarity(), eval_clarity(), close_eval_clarity();
extern int init_print_obj_tr_textloc(), print_obj_tr_textloc(), close_print_obj_tr_textloc();
//extern int init_proc_eval_clarity_topdocs(), proc_eval_clarity_topdocs(), close_proc_eval_clarity_topdocs();

static PROC_TAB proc_obj[] = {
    {"doctext",  init_print_obj_doctext, print_obj_text, close_print_obj_text},
    {"querytext",init_print_obj_querytext,print_obj_text,close_print_obj_text},
    {"proc",	init_print_obj_proc,	print_obj_proc,	close_print_obj_proc},
    {"dict",	init_print_obj_dict,	print_obj_dict,	close_print_obj_dict},
    {"inv",	init_print_obj_inv,	print_obj_inv,	close_print_obj_inv},
    {"invpos",	init_print_obj_invpos,	print_obj_invpos,	close_print_obj_invpos},
    {"rr_vec",	init_print_obj_rr_vec,print_obj_rr_vec,close_print_obj_rr_vec},
    {"textloc",  init_print_obj_textloc,print_obj_textloc,
                close_print_obj_textloc},
    {"eval",	init_print_obj_eval, print_obj_eval,
                close_print_obj_eval},
    {"qq_eval",	init_print_obj_qq_eval, print_obj_qq_eval,
                close_print_obj_qq_eval},
    {"ref_eval", init_print_obj_ref_eval, print_obj_ref_eval,
                close_print_obj_ref_eval},
    {"tr_vec",	init_print_obj_tr_vec,print_obj_tr_vec,close_print_obj_tr_vec},
    {"tr_wiki",	init_print_obj_wiki,print_obj_wiki,close_print_obj_wiki},
    {"tr_qd_textloc", init_print_obj_tr_textloc, print_obj_tr_textloc, close_print_obj_tr_textloc},
    {"vec",	init_print_obj_vec,	print_obj_vec,	close_print_obj_vec},
    {"vec_dict", init_print_obj_vec_dict,print_obj_vec_dict,
                close_print_obj_vec_dict},
    {"fdbk_stats",init_print_obj_fdbk_stats, print_obj_fdbk_stats,
	        close_print_obj_fdbk_stats},
    {"rel_header",init_print_obj_rel_header, print_obj_rel_header,
	        close_print_obj_rel_header},
    {"clarity", init_eval_clarity, eval_clarity, close_eval_clarity}
};
static int num_proc_obj =
    sizeof(proc_obj) / sizeof(proc_obj[0]);

extern int init_print_text_filter(), print_text_filter(), 
       close_print_text_filter();
extern int init_print_text_form(), print_text_form(), close_print_text_form();
extern int init_print_text_pp_named(), print_text_pp_named(), 
       close_print_text_pp_named();
extern int init_print_text_pp_next(), print_text_pp_next(), 
       close_print_text_pp_next();
static PROC_TAB proc_indivtext[] = {
    {"text_filter", init_print_text_filter, print_text_filter,   
                close_print_text_filter},
    {"text_form", init_print_text_form, print_text_form, close_print_text_form},
    {"text_pp_named", init_print_text_pp_named, print_text_pp_named, close_print_text_pp_named},
    {"text_pp_next", init_print_text_pp_next, print_text_pp_next, close_print_text_pp_next},
    };
static int num_proc_indivtext = sizeof (proc_indivtext) / sizeof (proc_indivtext[0]);


TAB_PROC_TAB proc_print[] = {
    {"top",       TPT_PROC, NULL, topproc_print,     &num_topproc_print},
    {"obj",       TPT_PROC, NULL, proc_obj,          &num_proc_obj},
    {"indivtext", TPT_PROC, NULL, proc_indivtext,    &num_proc_indivtext},
    };

int num_proc_print = sizeof (proc_print) / sizeof (proc_print[0]);



