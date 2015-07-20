#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfeedback/exp_const.c,v 11.2 1993/02/03 16:49:57 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "spec.h"
#include "trace.h"
#include "inst.h"
#include "smart_error.h"
#include "tr_vec.h"
#include "vector.h"
#include "feedback.h"

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Feedback expansion procedure - Add terms ocurring in percent of rel docs
 *1 feedback.expand.exp_percent
 *2 exp_percent (unused1, fdbk_info, inst)
 *3   char *unused1;
 *3   FEEDBACK_INFO *fdbk_info;
 *3   int inst;
 *4 init_exp_percent (spec, unused)
 *5   "feedback.query.num_ctypes"
 *5   "feedback.expand.trace"
 *5   "feedback.ctype.*.percent_expand"
 *4 close_exp_percent (inst)
 *7 Expand query by adding all terms occurring at least percent_expand
 *7 percentage of the rel docs.
 *7 Add all terms in rel docs, and then sort by decreasing freq within
 *7 rel docs.  Keep only orig query terms plus those with sufficient
 *7 occurrences.
***********************************************************************/

static long num_ctypes;

static SPEC_PARAM spec_args[] = {
    {"feedback.query.num_ctypes",     getspec_long,   (char *) &num_ctypes},
    TRACE_PARAM ("feedback.expand.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static char *prefix;
static float percent_expand;           /* percent of rel docs term must be in*/
static SPEC_PARAM_PREFIX prefix_spec_args[] = {
    {&prefix,   "percent_expand",     getspec_long,  (char *) &percent_expand}
    };
static int num_prefix_spec_args =
    sizeof (prefix_spec_args) / sizeof (prefix_spec_args[0]);


/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    int exp_rel_doc_inst;
    float *ctype_percent;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int init_exp_rel_doc(), exp_rel_doc(), close_exp_rel_doc();


int
init_expand_percent (spec, unused)
SPEC *spec;
char *unused;
{
    char param_prefix[40];
    long i;
    STATIC_INFO *ip;
    int new_inst;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_expand_percent");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];

    if (UNDEF == (ip->exp_rel_doc_inst =
                  init_exp_rel_doc (spec, (char *) NULL)))
        return (UNDEF);

    if (NULL == (ip->ctype_percent = (float *)
                 malloc ((unsigned) num_ctypes * sizeof (float))))
        return (UNDEF);

    for (i = 0; i < num_ctypes; i++) {
        (void) sprintf (param_prefix, "feedback.ctype.%ld.", i);
        prefix = param_prefix;
        if (UNDEF == lookup_spec_prefix (spec,
                                         &prefix_spec_args[0],
                                         num_prefix_spec_args))
            return (UNDEF);
        ip->ctype_percent[i] = percent_expand;
    }

    ip->valid_info = 1;
             
    PRINT_TRACE (2, print_string, "Trace: leaving init_expand_percent");

    return (new_inst);

}

int
expand_percent (unused, fdbk_info, inst)
char *unused;
FEEDBACK_INFO *fdbk_info;
int inst;
{
    STATIC_INFO *ip;
    OCC *occ_ptr, *new_occ_ptr, *end_occ;

    PRINT_TRACE (2, print_string, "Trace: entering expand_percent");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "skel");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Get all terms occurring in rel docs */
    if (UNDEF == exp_rel_doc (unused, fdbk_info, ip->exp_rel_doc_inst))
        return (UNDEF);

    if (fdbk_info->num_rel > 0) {
        occ_ptr = fdbk_info->occ;
        new_occ_ptr = fdbk_info->occ;
        end_occ = &fdbk_info->occ[fdbk_info->num_occ];
        while (occ_ptr < end_occ) {
            if (occ_ptr->query || 
                occ_ptr->rel_ret >= ((long)(ip->ctype_percent[occ_ptr->ctype] *
                                             fdbk_info->num_rel + 1.0))) {
                /* Move to new spot (if not already there) */
                if (occ_ptr != new_occ_ptr) {
                    (void) bcopy ((char *) occ_ptr,
                                  (char *) new_occ_ptr,
                                  sizeof (OCC));
                }
                new_occ_ptr++;
            }
            occ_ptr++;
        }
        
        fdbk_info->num_occ = new_occ_ptr - fdbk_info->occ;
    }

    PRINT_TRACE (4, print_fdbk_info, fdbk_info);
    PRINT_TRACE (2, print_string, "Trace: leaving expand_percent");
    return (1);
}

int
close_expand_percent (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_expand_percent");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_expand_percent");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (UNDEF == close_exp_rel_doc (ip->exp_rel_doc_inst))
        return (UNDEF);
    (void) free ((char *) ip->ctype_percent);

    PRINT_TRACE (2, print_string, "Trace: leaving close_expand_percent");
    return (0);
}
