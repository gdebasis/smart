#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libinter/show_exp.c,v 11.2 1993/02/03 16:51:29 smart Exp $";
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
#include "spec.h"
#include "inter.h"
#include "rr_vec.h"
#include "eval.h"
#include "inst.h"
#include "trace.h"
#include "smart_error.h"

static char *tr_file;
static long tr_mode;
static char *rr_file;
static long rr_mode;

static SPEC_PARAM spec_args[] = {
    {"retrieve.tr_file",           getspec_dbfile,   (char *) &tr_file},
    {"retrieve.tr_file.rmode",     getspec_filemode, (char *) &tr_mode},
    {"retrieve.rr_file",           getspec_dbfile,   (char *) &rr_file},
    {"retrieve.rr_file.rmode",     getspec_filemode, (char *) &rr_mode},
    TRACE_PARAM ("inter.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    SPEC *saved_spec;

    int tr_fd;
    int rr_fd;
    int rr_eval_inst;
    int tr_eval_inst;
    int ell_el_avg_inst;
    int ell_el_dev_inst;
    int ell_el_comp_over_inst;
    int ell_el_comp_overe_inst;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int init_spec_list();


int
init_show_exp (spec, is)
SPEC *spec;
INTER_STATE *is;
{
    STATIC_INFO *ip;
    int new_inst;
    long i;

    /* Check to see if this spec file is already initialized (only need
       one initialization per spec file) */
    for (i = 0; i <  max_inst; i++) {
        if (info[i].valid_info && spec == info[i].saved_spec) {
            info[i].valid_info++;
            return (i);
        }
    }
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_show_exp");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];

    ip->saved_spec = spec;

    /* Need to make sure spec_list has been initialized */
    if (UNDEF == init_spec_list (spec, is))
        return (UNDEF);

    /* Open files to be examined (Note that fd == UNDEF upon error) */
    ip->tr_fd = ip->rr_fd = UNDEF;
    if (VALID_FILE (tr_file))
        ip->tr_fd = open_tr_vec (tr_file, tr_mode);
    if (VALID_FILE (rr_file))
        ip->rr_fd = open_rr_vec (rr_file, rr_mode);

    /* Open evaluation procedures.  Note that these all take an argument */
    /* of the original spec file inter was invoked with, which may be */
    /* different from spec if an "Euse" command has been done */
    if (UNDEF == (ip->rr_eval_inst = 
                  init_rr_eval_list (is->orig_spec, (char *) NULL))||
        UNDEF == (ip->tr_eval_inst =
                  init_tr_eval_list (is->orig_spec, (char *) NULL))||
        UNDEF == (ip->ell_el_avg_inst = 
                  init_ell_el_avg (is->orig_spec, (char *) NULL)) ||
        UNDEF == (ip->ell_el_dev_inst = 
                  init_ell_el_dev (is->orig_spec, (char *) NULL)) ||
        UNDEF == (ip->ell_el_comp_over_inst = 
                  init_ell_el_comp_over (is->orig_spec, (char *) NULL)) ||
        UNDEF == (ip->ell_el_comp_overe_inst = 
                  init_ell_el_comp_over_equal (is->orig_spec,(char *) NULL))) {
        return (UNDEF);
    }

    ip->valid_info++;

    PRINT_TRACE (2, print_string, "Trace: leaving init_show_exp");

    return (new_inst);
}


int
close_show_exp (inst)
int inst;
{
    STATIC_INFO *ip;
    PRINT_TRACE (2, print_string, "Trace: entering close_show_exp");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.exp");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Check to see if still have valid instantiations using this data */
    if (--ip->valid_info > 0)
        return (0);

   /* Close files if open */
    if (ip->tr_fd != UNDEF && UNDEF == close_tr_vec (ip->tr_fd))
        return (UNDEF);
    if (ip->rr_fd != UNDEF && UNDEF == close_rr_vec (ip->rr_fd))
        return (UNDEF);
    if (UNDEF == close_rr_eval_list (ip->rr_eval_inst) ||
        UNDEF == close_tr_eval_list (ip->tr_eval_inst) ||
        UNDEF == close_ell_el_avg (ip->ell_el_avg_inst) ||
        UNDEF == close_ell_el_dev (ip->ell_el_dev_inst) ||
        UNDEF == close_ell_el_comp_over_equal (ip->ell_el_comp_overe_inst) ||
        UNDEF == close_ell_el_comp_over (ip->ell_el_comp_over_inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_show_exp");
    return (1);
}


int
show_exp_rr (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    RR_VEC rr_vec;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering show_exp_rr");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.exp");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (is->retrieval.query->qid == UNDEF) {
        if (UNDEF == add_buf_string ("No query specified\n", &is->err_buf))
            return (UNDEF);
        return (0);
    }
    if (ip->rr_fd == UNDEF) {
        if (UNDEF == add_buf_string ("rr file not found\n", &is->err_buf))
            return (UNDEF);
        return (0);
    }
    
    rr_vec.qid = is->retrieval.query->qid;
    if (1 != seek_rr_vec (ip->rr_fd, &rr_vec) ||
        1 != read_rr_vec (ip->rr_fd, &rr_vec)) {
        if (UNDEF == add_buf_string ("No relevant docs found\n", &is->err_buf))
            return (UNDEF);
        return (0);
    }

    if (UNDEF == add_buf_string ("Qid\tDid\tRank\tSim\n", &is->output_buf))
        return (UNDEF);
    print_rr_vec (&rr_vec, &is->output_buf);

    PRINT_TRACE (2, print_string, "Trace: leaving show_exp_rr");
    return (1);
}

int
show_exp_tr (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    TR_VEC tr_vec;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering show_exp_tr");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.exp");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (is->retrieval.query->qid == UNDEF) {
        if (UNDEF == add_buf_string ("No query specified\n", &is->err_buf))
            return (UNDEF);
        return (0);
    }
    if (ip->tr_fd == UNDEF) {
        if (UNDEF == add_buf_string ("tr file not found\n", &is->err_buf))
            return (UNDEF);
        return (0);
    }

    tr_vec.qid = is->retrieval.query->qid;
    if (1 != seek_tr_vec (ip->tr_fd, &tr_vec) ||
        1 != read_tr_vec (ip->tr_fd, &tr_vec)) {
        if (UNDEF == add_buf_string ("No top docs found\n", &is->err_buf))
            return (UNDEF);
        return (0);
    }

    if (UNDEF == add_buf_string ("Qid\tDid\tRank\tRel\tAction\tIter\tSim\n",
                                 &is->output_buf))
        return (UNDEF);
    print_tr_vec (&tr_vec, &is->output_buf);
    
    PRINT_TRACE (2, print_string, "Trace: leaving show_exp_tr");
    return (1);
}


int
show_exp_ind_rr (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering show_exp_ind_rr");
    print_ind_rr (&is->spec_list, &is->output_buf);
    PRINT_TRACE (2, print_string, "Trace: leaving show_exp_eval_rr");
    return (1);
}

int
show_exp_ind_tr (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering show_exp_ind_tr");
    print_ind_tr (&is->spec_list, &is->output_buf);
    PRINT_TRACE (2, print_string, "Trace: leaving show_exp_eval_tr");
    return (1);
}

/* Usage Eeval [ rr|tr [avg|...  [short|long|prec]]] */
int
show_exp_eval (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    EVAL_LIST_LIST eval_list_list;
    EVAL_LIST eval_list;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering show_exp_eval");
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.exp_eval");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (is->num_command_line < 2 || 0 == strcmp ("rr", is->command_line[1])) {
        if (UNDEF == rr_eval_list (&is->spec_list, &eval_list_list,
                                   ip->rr_eval_inst))
            return (UNDEF);
    }
    else if (0 == strcmp ("tr", is->command_line[1])) {
        if (UNDEF == tr_eval_list (&is->spec_list, &eval_list_list,
                                   ip->tr_eval_inst))
            return (UNDEF);
    }
    else {
       if (UNDEF == add_buf_string("Illegal eval datatype. Must be rr or tr\n",
                                   &is->err_buf))
            return (UNDEF);
        return (0);
    }

    if (is->num_command_line < 3 || 0 == strcmp ("avg", is->command_line[2])) {
        if (UNDEF == ell_el_avg (&eval_list_list,
                                 &eval_list,
                                 ip->ell_el_avg_inst))
            return (UNDEF);
    }
    else if (0 == strcmp ("dev", is->command_line[2])) {
        if (UNDEF == ell_el_dev (&eval_list_list,
                                 &eval_list,
                                 ip->ell_el_dev_inst))
            return (UNDEF);
    }
    else if (0 == strcmp ("comp_overequal", is->command_line[2])) {
        if (UNDEF == ell_el_comp_over_equal (&eval_list_list,
                                 &eval_list,
                                 ip->ell_el_comp_overe_inst))
            return (UNDEF);
    }
    else if (0 == strcmp ("comp_over", is->command_line[2])) {
        if (UNDEF == ell_el_comp_over (&eval_list_list,
                                 &eval_list,
                                 ip->ell_el_comp_over_inst))
            return (UNDEF);
    }
    else {
       if (UNDEF == add_buf_string("Illegal eval conversion. Must be one avg, dev, comp_over, comp_overequal\n",
                                   &is->err_buf))
            return (UNDEF);
        return (0);
    }
    
    if (is->num_command_line < 4 || 0 == strcmp ("short", is->command_line[3]))
        print_short_eval_list (&eval_list, &is->output_buf);
    else if (0 == strcmp ("long", is->command_line[3]))
        print_eval_list (&eval_list, &is->output_buf);
    else if (0 == strcmp ("prec", is->command_line[3]))
        print_prec_eval_list (&eval_list, &is->output_buf);
    else {
       if (UNDEF == add_buf_string("Illegal eval output. Must be short or long or prec\n",
                                   &is->err_buf))
            return (UNDEF);
        return (0);
    }
    
    PRINT_TRACE (2, print_string, "Trace: leaving show_exp_eval");
    return (1);
}

/* Usage Eq_eval [ rr|tr [short|long|prec]] */
int
show_exp_q_eval (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    EVAL_LIST_LIST eval_list_list;
    EVAL_LIST eval_list;
    long i,j;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering show_exp_q_eval");
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.exp_q_eval");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (is->retrieval.query->qid == UNDEF) {
        if (UNDEF == add_buf_string ("No query specified\n", &is->err_buf))
            return (UNDEF);
        return (0);
    }

    if (is->num_command_line < 2 || 0 == strcmp ("rr", is->command_line[1])) {
        if (UNDEF == rr_eval_list (&is->spec_list, &eval_list_list,
                                   ip->rr_eval_inst))
            return (UNDEF);
    }
    else if (0 == strcmp ("tr", is->command_line[1])) {
        if (UNDEF == tr_eval_list (&is->spec_list, &eval_list_list,
                                   ip->tr_eval_inst))
            return (UNDEF);
    }
    else {
       if (UNDEF == add_buf_string("Illegal eval datatype. Must be rr or tr\n",
                                   &is->err_buf))
            return (UNDEF);
        return (0);
    }

    /* Construct eval_list consisting of current query's result for each run */
    if (NULL == (eval_list.eval = (EVAL *)
              malloc ((unsigned) eval_list_list.num_eval_list * sizeof(EVAL))))
        return (UNDEF);
    eval_list.description = "   Single query evaluation";
    eval_list.num_eval = 0;
    for (i = 0; i < eval_list_list.num_eval_list; i++) {
        for (j = 0; j < eval_list_list.eval_list[i].num_eval; j++) {
            if (eval_list_list.eval_list[i].eval[j].qid == 
                                                   is->retrieval.query->qid) {
                bcopy ((char *) &eval_list_list.eval_list[i].eval[j],
                       (char *) &eval_list.eval[eval_list.num_eval],
                       sizeof (EVAL));
                eval_list.num_eval++;
                continue;
            }
        }
    }
    
    if (is->num_command_line < 3 || 0 == strcmp ("short", is->command_line[2]))
        print_short_eval_list (&eval_list, &is->output_buf);
    else if (0 == strcmp ("long", is->command_line[2]))
        print_eval_list (&eval_list, &is->output_buf);
    else if (0 == strcmp ("prec", is->command_line[2]))
        print_prec_eval_list (&eval_list, &is->output_buf);
    else {
       if (UNDEF == add_buf_string("Illegal eval output. Must be short or long or prec\n",
                                   &is->err_buf))
            return (UNDEF);
        return (0);
    }

    (void) free ((char *) &eval_list.eval);
    PRINT_TRACE (2, print_string, "Trace: leaving show_exp_q_eval");
    return (1);
}

int
show_exp_rclprn (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    EVAL_LIST_LIST eval_list_list;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering show_exp_rclprn");
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.exp_rclprn");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (is->num_command_line < 2 || 0 == strcmp ("rr", is->command_line[1])) {
        if (UNDEF == rr_eval_list (&is->spec_list, &eval_list_list,
                                   ip->rr_eval_inst))
            return (UNDEF);
    }
    else if (0 == strcmp ("tr", is->command_line[1])) {
        if (UNDEF == tr_eval_list (&is->spec_list, &eval_list_list,
                                   ip->tr_eval_inst))
            return (UNDEF);
    }
    else {
       if (UNDEF == add_buf_string("Illegal eval datatype. Must be rr or tr\n",
                                   &is->err_buf))
            return (UNDEF);
        return (0);
    }

    print_prec_eval_list_list (&eval_list_list, &is->output_buf);

    PRINT_TRACE (2, print_string, "Trace: leaving show_exp_rclprn");
    return (1);
}
