#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfeedback/form_best_query.c,v 11.0 1992/07/21 18:20:41 chrisb Exp $";
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


static PROC_INST best_terms;
static long num_ctypes;

static SPEC_PARAM spec_args[] = {
    {"feedback.num_ctypes",  getspec_long, (char *) &num_ctypes},
    {"feedback.best_terms",  getspec_func, (char *) &best_terms.ptab},
    TRACE_PARAM ("feedback.form_query.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static int comp_ctype_con();

int
init_form_best_query (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_form_best_query");

    if (UNDEF == (best_terms.inst =
                  best_terms.ptab->init_proc (spec, (char *) NULL)))
        return (UNDEF);


    PRINT_TRACE (2, print_string, "Trace: leaving init_form_best_query");
    return (1);
}

int
form_best_query (fdbk_info, new_query, inst)
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

    PRINT_TRACE (2, print_string, "Trace: entering form_best_query");
    PRINT_TRACE (6, print_fdbk_info, fdbk_info);

    max_ctype = -1;
    if (fdbk_info->num_occ > 0)
        max_ctype = fdbk_info->occ[fdbk_info->num_occ-1].ctype;

    if (max_ctype > num_ctypes) {
        set_error (SM_INCON_ERR,"Improper number of ctype", "form_best_query");
        return (UNDEF);
    }

    /* MEMORY LEAK */
    if (NULL == (con_wt = (CON_WT *) malloc ((unsigned) fdbk_info->num_occ
                                                 * sizeof (CON_WT))) ||
        NULL == (ctype_len = (long *) malloc ((unsigned) (1 + max_ctype)
                                               * sizeof (long))) ||
        NULL == (vec = (VEC *) malloc (sizeof (VEC)))) {
        set_error (errno, "", "form_best_query");
        return (UNDEF);
    }
    for (i = 0; i <= max_ctype; i++) {
        ctype_len[i] = 0;
    }

    conwt_ptr = con_wt;
    /* Must find best terms and just add those */
    /* best_terms will zero out all weights for terms */
    /* not to be included */
    if (UNDEF == best_terms.ptab->proc (fdbk_info, NULL, best_terms.inst))
        return (UNDEF);

    (void) qsort ((char *) fdbk_info->occ, fdbk_info->num_occ,
                  sizeof (OCC), comp_ctype_con);

    for (i = 0; i < fdbk_info->num_occ; i++) {
        if (fdbk_info->occ[i].weight > 0) {
            conwt_ptr->con = fdbk_info->occ[i].con;
            conwt_ptr->wt = fdbk_info->occ[i].weight;
            conwt_ptr++;
            ctype_len[fdbk_info->occ[i].ctype]++;
        }
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
    PRINT_TRACE (2, print_string, "Trace: leaving form_best_query");
    return (1);
}

int
close_form_best_query (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_form_best_query");

    if (UNDEF == best_terms.ptab->close_proc (best_terms.inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_form_best_query");
    return (1);
}

static int
comp_ctype_con(ptr1, ptr2)
OCC *ptr1;
OCC *ptr2;
{
    if (ptr1->ctype < ptr2->ctype)
        return (-1);
    if (ptr1->ctype > ptr2->ctype)
        return (1);
    if (ptr1->con < ptr2->con)
        return (-1);
    if (ptr1->con > ptr2->con)
        return (1);
    return (0);
}
