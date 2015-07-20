#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libproc/proc_print.c,v 11.0 1992/07/21 18:22:23 chrisb Exp chrisb $";
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

static PROC_TAB topproc_print[] = {
    {"empty",            INIT_EMPTY,     EMPTY,          CLOSE_EMPTY},
};
static int num_topproc_print =
    sizeof(topproc_print) / sizeof(topproc_print[0]);


extern int init_print_obj_collstat_dict(), print_obj_collstat_dict(), 
    close_print_obj_collstat_dict();
extern int init_print_obj_collstat_dict_noinfo(), 
    print_obj_collstat_dict_noinfo(), close_print_obj_collstat_dict_noinfo();
extern int init_print_obj_dict_noinfo(), 
    print_obj_dict_noinfo(), close_print_obj_dict_noinfo();
extern int init_print_obj_ltrie(), 
    print_obj_ltrie(), close_print_obj_ltrie();
extern int init_p_df_stats(), p_df_stats(), close_p_df_stats();
extern int init_l_print_obj_doctext(), init_l_print_obj_querytext(), 
    l_print_obj_text(), close_l_print_obj_text();
extern int l_init_print_obj_tr_vecdict(), l_print_obj_tr_vecdict(), l_close_print_obj_tr_vecdict();
extern int l_init_print_obj_tr_qdname(), l_print_obj_tr_qdname(), l_close_print_obj_tr_qdname();

static PROC_TAB proc_obj[] = {
    {"collstat_dict",	init_print_obj_collstat_dict,  print_obj_collstat_dict,
	close_print_obj_collstat_dict},
    {"collstat_dict_noinfo",	init_print_obj_collstat_dict_noinfo,  
      print_obj_collstat_dict_noinfo, close_print_obj_collstat_dict_noinfo},
    {"dict_noinfo",	init_print_obj_dict_noinfo,  
      print_obj_dict_noinfo, close_print_obj_dict_noinfo},
    {"ltrie",	init_print_obj_ltrie,  
      print_obj_ltrie, close_print_obj_ltrie},
    {"df_stats", init_p_df_stats, p_df_stats, close_p_df_stats},
    {"doctext", init_l_print_obj_doctext, l_print_obj_text, close_l_print_obj_text},
    {"querytext", init_l_print_obj_querytext, l_print_obj_text, close_l_print_obj_text},
    {"tr_vec_dict",  l_init_print_obj_tr_vecdict, l_print_obj_tr_vecdict, l_close_print_obj_tr_vecdict},	/* for printing the document names instead of the ids */
    {"tr_vec_qdname",  l_init_print_obj_tr_qdname, l_print_obj_tr_qdname, l_close_print_obj_tr_qdname}	/* for printing clef-ip runs */
};
static int num_proc_obj =
    sizeof(proc_obj) / sizeof(proc_obj[0]);


int init_print_text_mitre_named(), print_text_mitre_named(), close_print_text_mitre_named();
static PROC_TAB proc_indivtext[] = {
    {"text_mitre_named", init_print_text_mitre_named, print_text_mitre_named, close_print_text_mitre_named},
    {"empty",            INIT_EMPTY,     EMPTY,          CLOSE_EMPTY},
    };
static int num_proc_indivtext = sizeof (proc_indivtext) / sizeof (proc_indivtext[0]);


TAB_PROC_TAB lproc_print[] = {
   {"top",       TPT_PROC, NULL, topproc_print,     &num_topproc_print},
   {"obj",       TPT_PROC, NULL, proc_obj,          &num_proc_obj},
   {"indivtext", TPT_PROC, NULL, proc_indivtext,    &num_proc_indivtext},
    };

int num_lproc_print = sizeof (lproc_print) / sizeof (lproc_print[0]);
