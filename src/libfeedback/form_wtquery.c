#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfeedback/form_wtquery.c,v 11.0 1992/07/21 18:20:41 chrisb Exp $";
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

static char *prefix;
static long num_final;           /* Final number of query terms wanted */
static SPEC_PARAM_PREFIX prefix_spec_args[] = {
    {&prefix,   "num_final",      getspec_long,   (char *) &num_final},
    };
static int num_prefix_spec_args =
    sizeof (prefix_spec_args) / sizeof (prefix_spec_args[0]);

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
    SPEC *save_spec;
} STATIC_INFO;
 
static STATIC_INFO *info;
static int max_inst = 0;
 


static int compare_weight(), compare_con();

int
init_form_wtquery (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;
 
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_form_wtquery");
    /* Reserve space for this new instantiation's static variables */
    NEW_INST (new_inst);
    if (new_inst == UNDEF)
        return (UNDEF);
 
    ip = &info[new_inst];
    
    ip->num_ctype = 0;
    ip->num_conwt = 0;
    ip->save_spec = spec;

    ip->valid_info = 1;
    

    PRINT_TRACE (2, print_string, "Trace: leaving init_form_wtquery");
    return (1);
}

int
form_wtquery (fdbk_info, new_query, inst)
FEEDBACK_INFO *fdbk_info;
QUERY_VECTOR *new_query;
int inst;
{
    STATIC_INFO *ip;
    long i;
    long max_ctype;
    CON_WT *conwt_ptr;
    VEC *vec;
    char param_prefix[40];
    OCC *start_occ_ctype, *end_occ_ctype, *end_occ;
    long num_this_ctype;

    PRINT_TRACE (2, print_string, "Trace: entering form_wtquery");
    PRINT_TRACE (6, print_fdbk_info, fdbk_info);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "form_query");
        return (UNDEF);
    }
    ip  = &info[inst];
 
    max_ctype = -1;
    if (fdbk_info->num_occ > 0)
        max_ctype = fdbk_info->occ[fdbk_info->num_occ-1].ctype;

    /* Make sure enough space reserved for concept,weight pairs */
    if (fdbk_info->num_occ > ip->num_conwt) {
        if (ip->num_conwt > 0)
            (void) free ((char *) ip->con_wtp);
        ip->num_conwt += fdbk_info->num_occ;
        if (NULL == (ip->con_wtp = (CON_WT *)
                     malloc ((unsigned) ip->num_conwt * sizeof (CON_WT)))) {
            return (UNDEF);
        }
    }
    if (max_ctype >= ip->num_ctype && max_ctype >= 0) {
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
    vec->id_num = fdbk_info->orig_query->id_num;
    
    for (i = 0; i <= max_ctype; i++) {
        vec->ctype_len[i] = 0;
    }
 
    conwt_ptr = vec->con_wtp;

    start_occ_ctype = fdbk_info->occ;
    end_occ = &fdbk_info->occ[fdbk_info->num_occ];
    while (start_occ_ctype < end_occ) {
        end_occ_ctype = start_occ_ctype + 1;
        while (end_occ_ctype < end_occ &&
               end_occ_ctype->ctype == start_occ_ctype->ctype)
            end_occ_ctype++;
        num_this_ctype = end_occ_ctype - start_occ_ctype;

        /* Get number wanted for this ctype */
        (void) sprintf (param_prefix,
                        "feedback.ctype.%ld.",
                        start_occ_ctype->ctype);
        prefix = param_prefix;
        if (UNDEF == lookup_spec_prefix (ip->save_spec,
                                         &prefix_spec_args[0],
                                         num_prefix_spec_args))
            return (UNDEF);
        if (num_this_ctype > num_final) {
            /* Must find the highest weighted terms and just add those */
            qsort ((char *) start_occ_ctype,
                   (int) num_this_ctype,
                   sizeof (OCC),
                   compare_weight);
            num_this_ctype = num_final;
            qsort ((char *) start_occ_ctype,
                   (int) num_this_ctype, sizeof (OCC), compare_con);
        }

        for (i = 0; i < num_this_ctype; i++) {
            conwt_ptr->con = start_occ_ctype[i].con;
            conwt_ptr->wt = start_occ_ctype[i].weight;
            conwt_ptr++;
        }
        vec->ctype_len[start_occ_ctype->ctype] = num_this_ctype;

        start_occ_ctype = end_occ_ctype;
    }

    vec->num_conwt = conwt_ptr - vec->con_wtp;

    /* Cosine normalize the entire query */
    if (UNDEF == normwt_cos (vec, (char *) NULL, 0))
        return (UNDEF);

    new_query->qid = vec->id_num.id;
    new_query->vector = (char *) vec;

    PRINT_TRACE (4, print_vector, vec);
    PRINT_TRACE (2, print_string, "Trace: leaving form_wtquery");
    return (1);
}

int
close_form_wtquery (inst)
int inst;
{
    STATIC_INFO *ip;
    PRINT_TRACE (2, print_string, "Trace: entering close_form_wtquery");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_form_wtquery");
        return (UNDEF);
    }
    ip  = &info[inst];
 
    if (ip->num_conwt > 0) 
        (void) free ((char *) ip->con_wtp);
    if (ip->num_ctype > 0) 
        (void) free ((char *) ip->ctype_len);
 
    ip->valid_info = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving close_form_wtquery");
    return (1);
}


static int
compare_con (ptr1, ptr2)
OCC *ptr1;
OCC *ptr2;
{
    if (ptr1->con < ptr2->con)
        return (-1);
    if (ptr1->con > ptr2->con)
        return (1);
    return (0);
}

static int
compare_weight (ptr1, ptr2)
OCC *ptr1;
OCC *ptr2;
{
    if (ptr1->weight > ptr2->weight)
        return (-1);
    if (ptr1->weight < ptr2->weight)
        return (1);
    return (0);
}
