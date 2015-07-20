#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfeedback/form_query.c,v 11.0 1992/07/21 18:20:41 chrisb Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "vector.h"
#include "feedback.h"
#include "smart_error.h"
#include "spec.h"
#include "inst.h"
#include "trace.h"
#include "retrieve.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("feedback.form_query.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    VEC vec;
    /* ctype info */
    long *ctype_len;         /* lengths of each subvector in vec */
    long num_ctype;          /* Number of sub-vectors space reserved for */
    /* conwt_info */
    CON_WT *con_wtp;         /* pointer to concepts,weights for vec */
    long num_conwt;          /* total number of concept,weight pairs
                                space reserved for */
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_form_query (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_form_query");

    /* Reserve space for this new instantiation's static variables */
    NEW_INST (new_inst);
    if (new_inst == UNDEF)
        return (UNDEF);

    ip = &info[new_inst];
    
    ip->num_ctype = 0;
    ip->num_conwt = 0;
    ip->valid_info = 1;
    
    PRINT_TRACE (2, print_string, "Trace: leaving init_form_query");
    return (new_inst);
}

int
form_query (fdbk_info, new_query, inst)
FEEDBACK_INFO *fdbk_info;
QUERY_VECTOR *new_query;
int inst;
{
    STATIC_INFO *ip;
    long i;
    long max_ctype;
    CON_WT *conwt_ptr;
    VEC *vec;
    long num_conwt;


    PRINT_TRACE (2, print_string, "Trace: entering form_query");
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "form_query");
        return (UNDEF);
    }
    ip  = &info[inst];

    num_conwt = 0; max_ctype = -1;
    for (i = 0; i < fdbk_info->num_occ; i++) {
        if (fdbk_info->occ[i].ctype > max_ctype)
            max_ctype = fdbk_info->occ[i].ctype;
        if (fdbk_info->occ[i].weight != 0.0)
            num_conwt++;
    }

    /* Make sure enough space reserved for concept,weight pairs */
    if (num_conwt > ip->num_conwt) {
        if (ip->num_conwt > 0)
            (void) free ((char *) ip->con_wtp);
        ip->num_conwt += num_conwt;
        if (NULL == (ip->con_wtp = (CON_WT *)
                     malloc ((unsigned) ip->num_conwt * sizeof (CON_WT)))) {
            return (UNDEF);
        }
    }
    if (max_ctype >= ip->num_ctype) {
        if (ip->num_ctype > 0)
            (void) free ((char *) ip->ctype_len);
        ip->num_ctype += max_ctype + 1;
        if (NULL == (ip->ctype_len = (long *)
                     malloc ((unsigned) ip->num_ctype * sizeof (long)))) {
            return (UNDEF);
        }
    }

    vec = &ip->vec;
    vec->ctype_len = ip->ctype_len;
    vec->con_wtp = ip->con_wtp;
    vec->num_ctype = max_ctype + 1;
    vec->num_conwt = num_conwt;
    vec->id_num = fdbk_info->orig_query->id_num;
    
    for (i = 0; i <= max_ctype; i++) {
        vec->ctype_len[i] = 0;
    }

    conwt_ptr = vec->con_wtp;
    for (i = 0; i < fdbk_info->num_occ; i++) {
        if (fdbk_info->occ[i].weight == 0.0)
            continue;

        conwt_ptr->con = fdbk_info->occ[i].con;
        conwt_ptr->wt = fdbk_info->occ[i].weight;
        vec->ctype_len[fdbk_info->occ[i].ctype]++;
        conwt_ptr++;
    }

    if (UNDEF == normwt_cos (vec, (char *) NULL, 0))
        return (UNDEF);

    new_query->qid = vec->id_num.id;
    new_query->vector = (char *) vec;

    
    PRINT_TRACE (4, print_vector, vec);
    PRINT_TRACE (2, print_string, "Trace: leaving form_query");
    return (1);
}

int
close_form_query (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_form_query");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_form_query");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (ip->num_conwt > 0) 
        (void) free ((char *) ip->con_wtp);
    if (ip->num_ctype > 0) 
        (void) free ((char *) ip->ctype_len);

    ip->valid_info = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving close_form_query");
    return (1);
}
