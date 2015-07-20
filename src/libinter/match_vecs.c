#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libinter/match_vecs.c,v 11.2 1993/02/03 16:51:25 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "textloc.h"
#include "io.h"
#include "smart_error.h"
#include "spec.h"
#include "sm_display.h"
#include "tr_vec.h"
#include "context.h"
#include "retrieve.h"
#include "docindex.h"
#include "vector.h"
#include "docdesc.h"
#include "inter.h"
#include "trace.h"
#include "inst.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("inter.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);


/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    SPEC *saved_spec;
    
    SM_INDEX_DOCDESC doc_desc;
    int *contok_inst;
    int util_inst;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;


static int get_vec();

int
init_match_vec (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;
    long i;
    char param_prefix[PATH_LEN];

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_match_vec");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];

    if (UNDEF == lookup_spec_docdesc (spec, &ip->doc_desc))
        return (UNDEF);

    /* Reserve space for the instantiation ids of the called procedures. */
    if (NULL == (ip->contok_inst = (int *)
                 malloc ((unsigned) ip->doc_desc.num_ctypes * sizeof (int))))
        return (UNDEF);

     for (i = 0; i < ip->doc_desc.num_ctypes; i++) {
        /* Set param_prefix to be current parameter path for this ctype.
           This will then be used by the con_to_token routine to lookup
           parameters it needs. */
        (void) sprintf (param_prefix, "index.ctype.%ld.", i);
        if (UNDEF == (ip->contok_inst[i] =
                      ip->doc_desc.ctypes[i].con_to_token->init_proc
                      (spec, param_prefix)))
            return (UNDEF);
    }

    if (UNDEF == (ip->util_inst = init_inter_util (spec, (char *) NULL)))
        return (UNDEF);

    ip->valid_info++;

    PRINT_TRACE (2, print_string, "Trace: leaving init_match_vec");

    return (new_inst);
}


int
close_match_vec (inst)
int inst;
{
    long i;
    STATIC_INFO *ip;
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.match_vecs");
        return (UNDEF);
    }
    ip  = &info[inst];

    for (i = 0; i < ip->doc_desc.num_ctypes; i++) {
        if (UNDEF == ip->doc_desc.ctypes[i].con_to_token->
            close_proc(ip->contok_inst[i]))
            return (UNDEF);
    }

    if (UNDEF == close_inter_util (ip->util_inst))
        return (UNDEF);

    (void) free ((char *) ip->contok_inst);
    ip->valid_info--;

    return (1);
}

static int
match_vec (is, vec1, vec2, inst)
INTER_STATE *is;
VEC *vec1, *vec2;
int inst;
{
    long ctype;                /* Current ctype checking for match */
    CON_WT *conwt1, *conwt2;   /* Current concepts checking */
    CON_WT *end_ctype1, *end_ctype2; /* Last concept for this ctype */
    char temp_buf[PATH_LEN];
    char *token;
    int match_flag = 0;
    STATIC_INFO *ip;
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.match_vecs");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (UNDEF == add_buf_string ("Ctype\tCon\tVec1\tVec2\t\tProduct\tToken\n",
                                 &is->output_buf))
        return (UNDEF);
    conwt1 = vec1->con_wtp;
    conwt2 = vec2->con_wtp;
    for (ctype = 0;
         ctype < vec1->num_ctype && ctype < vec2->num_ctype; 
         ctype++) {
        end_ctype1 = &conwt1[vec1->ctype_len[ctype]];
        end_ctype2 = &conwt2[vec2->ctype_len[ctype]];
        while (conwt1 < end_ctype1 && conwt2 < end_ctype2) {
            if (conwt1->con < conwt2->con)
                conwt1++;
            else if (conwt1->con > conwt2->con)
                conwt2++;
            else {
                /* have match */
                if (UNDEF == ip->doc_desc.ctypes[ctype].con_to_token->
                    proc (&conwt1->con, &token, ip->contok_inst[ctype])) {
                    token = "Not in dictionary";
                    clr_err();
                }
               (void) sprintf (temp_buf,
                                "%ld\t%ld\t%6.4f\t%6.4f\t\t%6.4f\t%s\n",
                                ctype, conwt1->con, conwt1->wt, conwt2->wt,
                                conwt1->wt * conwt2->wt,
                                token);
                if (UNDEF == add_buf_string (temp_buf, &is->output_buf))
                    return (UNDEF);
                conwt1++; conwt2++;
                match_flag++;
            }
        }
        conwt1 = end_ctype1;
        conwt2 = end_ctype2;
    }

    if (match_flag == 0)
        if (UNDEF == add_buf_string ("No matching concepts",&is->output_buf))
            return (UNDEF);

    return (1);
}


int
match_qvec (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    char tmp_buf[PATH_LEN];
    int status, i;
    VEC vec2;

    PRINT_TRACE (2, print_string, "Trace: entering match_qvec");

    if (is->num_command_line < 2) {
        if (UNDEF == add_buf_string ("Must specify a doc", &is->err_buf))
            return (UNDEF);
        return (0);
    }
    if (NULL == is->retrieval.query->vector) {
        if (UNDEF == add_buf_string ("No query specified", &is->err_buf))
            return (UNDEF);
        return (0);
    }

    for (i = 1; i < is->num_command_line; i++) {
	sprintf(tmp_buf, "\n-----\n%s\n-----\n", is->command_line[i]);
	if (UNDEF == add_buf_string (tmp_buf, &is->output_buf))
            return (UNDEF);
	if (1 != (status = get_vec (is, is->command_line[i], &vec2, inst)) ||
	    UNDEF == (status=match_vec (is, (VEC *)is->retrieval.query->vector,
					&vec2, inst)))
	    return(status);
	if (UNDEF == free_vec (&vec2))
	    return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving match_qvec");

    return (1);
}


int
match_dvec (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    int status;
    VEC vec1, vec2;

    PRINT_TRACE (2, print_string, "Trace: entering match_dvec");

    if (is->num_command_line < 3) {
        if (UNDEF == add_buf_string ("Must specify two docs", &is->err_buf))
            return (UNDEF);
        return (0);
    }

    if (1 != (status = get_vec (is,
                                is->command_line[1],
                                &vec1,
                                inst)))
        return (status);
    if (1 != (status = get_vec (is, 
                                is->command_line[2],
                                &vec2,
                                inst)))
        return (status);

    status = match_vec (is, &vec1, &vec2, inst);
    
    if (UNDEF == free_vec (&vec1))
        return (UNDEF);
    if (UNDEF == free_vec (&vec2))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving match_dvec");

    return (status);
}


static int
get_vec (is, command_line, vec, inst)
INTER_STATE *is;
char *command_line;
VEC *vec;
int inst;
{
    VEC temp_vec;
    STATIC_INFO *ip;
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.match_vecs");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (UNDEF == inter_get_vec (command_line, &temp_vec, ip->util_inst)) {
        if (UNDEF == add_buf_string ("Not a valid single document. Command ignored\n",
                                     &is->err_buf))
            return (UNDEF);
    }
    
    if (UNDEF == copy_vec (vec, &temp_vec))
        return (UNDEF);

    return (1);
}
