#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libproc/proc_convert.c,v 11.2 1993/02/03 16:52:22 smart Exp $";
#endif

/* Copyright (c) 1996,1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "proc.h"

static PROC_TAB topproc_pnorm[] = {
    {"empty",            INIT_EMPTY,     EMPTY,          CLOSE_EMPTY}
};
static int num_topproc_pnorm =
    sizeof(topproc_pnorm) / sizeof(topproc_pnorm[0]);

extern int init_sim_ctype_inv_p(), sim_ctype_inv_p(), close_sim_ctype_inv_p();
/* extern int init_inv_eval_p(), inv_eval_p(), close_inv_eval_p(); */
extern int inv_eval_p();
static PROC_TAB proc_retrieve[] = {
    {"inv_eval_p",     INIT_EMPTY, inv_eval_p, CLOSE_EMPTY},
    {"sim_ctype_inv_p",     init_sim_ctype_inv_p, sim_ctype_inv_p, close_sim_ctype_inv_p},
};
static int num_proc_retrieve =
    sizeof(proc_retrieve) / sizeof(proc_retrieve[0]);

static PROC_TAB proc_convert[] = {
    {"empty",            INIT_EMPTY,     EMPTY,          CLOSE_EMPTY}
    };
static int num_proc_convert = sizeof (proc_convert) / sizeof (proc_convert[0]);

static PROC_TAB proc_print[] = {
    {"empty",            INIT_EMPTY,     EMPTY,          CLOSE_EMPTY}
    };
static int num_proc_print = sizeof (proc_print) / sizeof (proc_print[0]);

static PROC_TAB proc_feedback[] = {
    {"empty",            INIT_EMPTY,     EMPTY,          CLOSE_EMPTY}
    };
static int num_proc_feedback = sizeof (proc_feedback) / sizeof (proc_feedback[0]);


TAB_PROC_TAB lproc_pnorm[] = {
    {"top",  TPT_PROC, NULL, topproc_pnorm, &num_topproc_pnorm},
    {"retrieve",  TPT_PROC, NULL, proc_retrieve, &num_proc_retrieve},
    {"convert",      TPT_PROC, NULL, proc_convert,       &num_proc_convert},
    {"print",      TPT_PROC, NULL, proc_print,       &num_proc_print},
    {"feedback",      TPT_PROC, NULL, proc_feedback,       &num_proc_feedback},
    };
int num_lproc_pnorm = sizeof (lproc_pnorm) / sizeof (lproc_pnorm[0]);
