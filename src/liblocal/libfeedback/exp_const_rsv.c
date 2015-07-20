#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfeedback/local_exp_const_rsv.c,v 11.2 1993/02/03 16:49:57 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "tr_vec.h"
#include "vector.h"
#include "feedback.h"
#include "spec.h"
#include "trace.h"

/* Expand query by adding at least num_expand terms occurring in rel docs.
   Add all terms in rel docs, and then sort by decreasing freq within
   rel docs, and then decreasing weight.  Keep only best num_expand +
   sizeof orig_query terms.

   Query terms are always in the new query.
   Ctypes are ignored.

   The difference of local.feedback.local_exp_const_rsv with feedback.exp_const_rsv is that
   it uses the rel_weight to order the terms. The function local.feedback.exp_rel_doc
   stores the RSV weights.
*/


static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("feedback.expand.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static char *prefix;
static long num_expand;           /* Number of terms to add */
static SPEC_PARAM_PREFIX prefix_spec_args[] = {
    {&prefix,   "num_expand",      getspec_long,   (char *) &num_expand},
    };
static int num_prefix_spec_args =
    sizeof (prefix_spec_args) / sizeof (prefix_spec_args[0]);



static int compare_con(), compare_occ_info();

static int local_exp_rel_doc_inst;

int init_local_exp_reldoc_rsv(), local_exp_reldoc_rsv(), close_local_exp_reldoc_rsv();

static SPEC *save_spec;


int
init_local_exp_const_rsv (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_local_exp_const_rsv");

    if (UNDEF == (local_exp_rel_doc_inst = init_local_exp_reldoc_rsv (spec, (char *) NULL)))
        return (UNDEF);

    save_spec = spec;

    PRINT_TRACE (2, print_string, "Trace: leaving init_local_exp_const_rsv");
    return (1);
}

int
local_exp_const_rsv (unused, fdbk_info, inst)
char *unused;
FEEDBACK_INFO *fdbk_info;
int inst;
{
    OCC *start_ctype_occ, *end_ctype_occ, *end_occ, *end_good_occ;
    long num_orig_query;
    long num_this_ctype;
    char param_prefix[40];

    PRINT_TRACE (2, print_string, "Trace: entering local_exp_const_rsv");

    /* Get all terms occurring in rel docs */
    if (UNDEF == local_exp_reldoc_rsv (unused, fdbk_info, local_exp_rel_doc_inst))
        return (UNDEF);

    if (fdbk_info->num_rel > 0) {
        PRINT_TRACE (8, print_fdbk_info, fdbk_info);
        /* Go through expanded terms ctype by ctype */
        start_ctype_occ = fdbk_info->occ;
        end_good_occ = fdbk_info->occ;
        end_occ = &fdbk_info->occ[fdbk_info->num_occ];
        while (start_ctype_occ < end_occ) {
            end_ctype_occ = start_ctype_occ + 1;
            num_orig_query = (start_ctype_occ->query ? 1 : 0);
            while (end_ctype_occ < end_occ &&
                   end_ctype_occ->ctype == start_ctype_occ->ctype) {
                if (end_ctype_occ->query)
                    num_orig_query++;
                end_ctype_occ++;
            }

            num_this_ctype = end_ctype_occ - start_ctype_occ;
            /* Sort the occurrence info by
             *  1. query  (all original query terms occur first)
             *  2. rel_ret  (those terms occurring in most rel docs occur next)
             *  3. rel_weight (those terms with highest avg weight)
             */
            qsort ((char *) start_ctype_occ,
                   (int) num_this_ctype,
                   sizeof (OCC),
                   compare_occ_info);

            (void) sprintf (param_prefix,
                            "feedback.ctype.%ld.",
                            start_ctype_occ->ctype);
            prefix = param_prefix;
            if (UNDEF == lookup_spec_prefix (save_spec,
                                             &prefix_spec_args[0],
                                             num_prefix_spec_args))
                return (UNDEF);

            PRINT_TRACE (8, print_fdbk_info, fdbk_info);
            /* Set number of terms to be orig_query length plus num_expand */
            if (num_this_ctype > num_orig_query + num_expand)
                num_this_ctype = num_orig_query + num_expand;

            /* Resort by con */
            qsort ((char *) start_ctype_occ,
               (int) num_this_ctype, sizeof (OCC), compare_con);
            /* Copy into good portion of occ array (skip copying if already
               in correct location) */
            if (start_ctype_occ != end_good_occ) {
                (void) bcopy ((char *) start_ctype_occ,
                              (char *) end_good_occ,
                              sizeof (OCC) * (int) num_this_ctype);
            }
            end_good_occ += num_this_ctype;
            start_ctype_occ = end_ctype_occ;
        }
        fdbk_info->num_occ = end_good_occ - fdbk_info->occ;
    }

    PRINT_TRACE (4, print_fdbk_info, fdbk_info);
    PRINT_TRACE (2, print_string, "Trace: leaving local_exp_const_rsv");
    return (1);
}

int
close_local_exp_const_rsv (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_local_exp_const_rsv");

    if (UNDEF == close_local_exp_reldoc_rsv (local_exp_rel_doc_inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_local_exp_const_rsv");
    return (0);
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
compare_occ_info (ptr1, ptr2)
OCC *ptr1;
OCC *ptr2;
{
    if (ptr1->query) {
        if (ptr2->query)
            return (0);
        return (-1);
    }
    if (ptr2->query)
        return (1);
    return (ptr1->rel_weight > ptr2->rel_weight)? -1 : ptr1->rel_weight == ptr2->rel_weight? 0 : 1;
    return (0);
}
