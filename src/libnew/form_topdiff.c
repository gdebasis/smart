#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfeedback/form_wtquery
.c,v 11.0 1992/07/21 18:20:41 chrisb Exp $";
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

SPEC *save_spec;

static int compare_weight(), compare_con();

int
init_form_wtquery (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_form_wtquery");

    save_spec = spec;

    PRINT_TRACE (2, print_string, "Trace: leaving init_form_wtquery");
    return (1);
}

int
form_wtquery (fdbk_info, new_query, inst)
FEEDBACK_INFO *fdbk_info;
QUERY_VECTOR *new_query;
int inst;
{
    long i;
    long max_ctype;
    CON_WT *conwt_ptr;
    VEC *vec;
    CON_WT *con_wt;
    long *ctype_len;
    char param_prefix[40];
    OCC *start_occ_ctype, *end_occ_ctype, *end_occ;
    long num_this_ctype;

    PRINT_TRACE (2, print_string, "Trace: entering form_wtquery");
    PRINT_TRACE (6, print_fdbk_info, fdbk_info);

    max_ctype = -1;
    if (fdbk_info->num_occ > 0)
        max_ctype = fdbk_info->occ[fdbk_info->num_occ-1].ctype;

    /* MEMORY LEAK */
    if (NULL == (con_wt = (CON_WT *) malloc ((unsigned) fdbk_info->num_occ
                                                 * sizeof (CON_WT))) ||
        NULL == (ctype_len = (long *) malloc ((unsigned) (1 + max_ctype)
                                               * sizeof (long))) ||
        NULL == (vec = (VEC *) malloc (sizeof (VEC)))) {
        set_error (errno, "", "form_wtquery");
        return (UNDEF);
    }
    for (i = 0; i <= max_ctype; i++) {
        ctype_len[i] = 0;
    }

    conwt_ptr = con_wt;
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
        if (UNDEF == lookup_spec_prefix (save_spec,
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
        ctype_len[start_occ_ctype->ctype] = num_this_ctype;

        start_occ_ctype = end_occ_ctype;
    }

    vec->ctype_len = ctype_len;
    vec->con_wtp = con_wt;
    vec->num_ctype = max_ctype + 1;
    vec->num_conwt = conwt_ptr - con_wt;
    vec->id_num = fdbk_info->orig_query->id_num;

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
    PRINT_TRACE (2, print_string, "Trace: entering close_form_wtquery");

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
