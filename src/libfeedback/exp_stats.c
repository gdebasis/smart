#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfeedback/exp_const_stats.c,v 11.0 1992/07/21 18:20:39 chrisb Exp $";
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
#include "fdbk_stats.h"
#include "spec.h"
#include "trace.h"
/********************   PROCEDURE DESCRIPTION   ************************
 *0 Feedback expansion procedure - Use feedback statistical file.
 *1 feedback.expand.exp_const_stats
 *2 exp_const_stats (unused1, fdbk_info, inst)
 *3   char *unused1;
 *3   FEEDBACK_INFO *fdbk_info;
 *3   int inst;
 *4 init_exp_const_stats (spec, unused)
 *5    "feedback.fdbk_stat_file"
 *5   "feedback.fdbk_stat_file.rmode"
 *5   "feedback.expand.trace"
 *5   "feedback.ctype.*.num_expand"
 *4 close_exp_const_stats (inst)
 *7 Expand query by adding at least num_expand terms occurring in rel docs.
 *7 Add all terms in rel docs, and then sort by decreasing freq within
 *7 rel docs, and then decreasing weight.  Keep only best num_expand +
 *7 sizeof orig_query terms.
 *7 Query terms are always in the new query.
 *7 Occurrence statistics gathered from query dependent feedback_stats.
***********************************************************************/

static char *fdbk_stats_file;
static long fdbk_stats_rmode;

static SPEC_PARAM spec_args[] = {
    {"feedback.fdbk_stats_file",  getspec_dbfile, (char *) &fdbk_stats_file},
    {"feedback.fdbk_stats_file.rmode",getspec_filemode,(char *)&fdbk_stats_rmode},
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

static int fdbk_fd;

static int compare_con(), compare_occ_info();

static int merge_query();

static SPEC *save_spec;


int
init_exp_const_stats (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_exp_const_stats");

    if (! VALID_FILE(fdbk_stats_file) ||
        UNDEF == (fdbk_fd = open_fdbkstats (fdbk_stats_file,
                                            fdbk_stats_rmode)))
        return (UNDEF);

    save_spec = spec;

    PRINT_TRACE (2, print_string, "Trace: leaving init_exp_const_stats");
    return (1);
}

int
exp_const_stats (unused, fdbk_info, inst)
char *unused;
FEEDBACK_INFO *fdbk_info;
int inst;
{
    OCC *start_ctype_occ, *end_ctype_occ, *end_occ, *end_good_occ;
    long num_orig_query;
    long num_this_ctype;
    char param_prefix[40];
    FDBK_STATS fdbk_stats;
    int status;

    PRINT_TRACE (2, print_string, "Trace: entering exp_const_stats");

    /* read previous info for relevance occurrences for this query */
    /* if no info, set fdbk_stats.id_num to UNDEF */
    fdbk_stats.id_num = fdbk_info->orig_query->id_num.id;
    if (1 != (status =seek_fdbkstats (fdbk_fd, &fdbk_stats))) {
        if (status == UNDEF)
            return (UNDEF);
        fdbk_stats.id_num = UNDEF;
    }
    else {
        if (1 != (status = read_fdbkstats (fdbk_fd, &fdbk_stats))) {
            if (status == UNDEF)
                return (UNDEF);
            fdbk_stats.id_num = UNDEF;
        }
    }

    /* Merge info from query with previous stats, putting into */
    /* fdbk_info->occ */
    if (UNDEF == merge_query (fdbk_info, &fdbk_stats))
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
    PRINT_TRACE (2, print_string, "Trace: leaving exp_const_stats");
    return (1);
}

int
close_exp_const_stats (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_exp_const_stats");

    if (UNDEF == close_fdbkstats (fdbk_fd))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_exp_const_stats");
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
    if (ptr1->rel_ret > ptr2->rel_ret)
        return (-1);
    if (ptr1->rel_ret < ptr2->rel_ret)
        return (1);
    if (ptr1->rel_weight > ptr2->rel_weight)
        return (-1);
    if (ptr1->rel_weight < ptr2->rel_weight)
        return (1);
    return (0);
}


static int
merge_query (fdbk_info, fdbk_stats)
FEEDBACK_INFO *fdbk_info;
FDBK_STATS *fdbk_stats;
{
    CON_WT *conwt, *end_conwt;        /* Current concept within query */
    OCCINFO *occinfo, *end_occinfo;   /* Current concept within occinfo */
    OCC *new_occ;                     /* Current concept within the
                                         merged OCC relation */
    VEC *qvec;
    long ctype;

    if (fdbk_stats->id_num == UNDEF)
        fdbk_stats->num_occinfo = 0;

    qvec = fdbk_info->orig_query;
    if (fdbk_info->max_occ < qvec->num_conwt + fdbk_stats->num_occinfo) {
        if (fdbk_info->max_occ > 0)
            (void) free ((char *) fdbk_info->occ);
        fdbk_info->max_occ = qvec->num_conwt + fdbk_stats->num_occinfo;
        if (NULL == (fdbk_info->occ = (OCC *)
                     malloc ((unsigned) fdbk_info->max_occ * sizeof (OCC))))
            return (UNDEF);
    }

    (void) bzero ((char *) fdbk_info->occ,
                  fdbk_info->max_occ * sizeof (OCC));

    conwt = qvec->con_wtp;
    occinfo = fdbk_stats->occinfo;
    end_occinfo = &fdbk_stats->occinfo[fdbk_stats->num_occinfo];
    new_occ = fdbk_info->occ;

    for (ctype = 0; ctype < qvec->num_ctype; ctype++) {
        end_conwt = &conwt[qvec->ctype_len[ctype]];
        while (conwt < end_conwt && occinfo < end_occinfo) {
            if (ctype < occinfo->ctype ||
                (ctype == occinfo->ctype && conwt->con < occinfo->con)) {
                /* Add current query concept */
                new_occ->query = 1;
                new_occ->con = conwt->con;
                new_occ->ctype = ctype;
                new_occ->orig_weight = conwt->wt;
                conwt++;
            }
            else if (ctype == occinfo->ctype && conwt->con == occinfo->con) {
                /* Merge old and new info */
                new_occ->query = 1;
                new_occ->con = conwt->con;
                new_occ->ctype = ctype;
                new_occ->rel_ret = occinfo->rel_ret;
                new_occ->nrel_ret = occinfo->nrel_ret;
                new_occ->nret = occinfo->nret;
                new_occ->orig_weight = conwt->wt;
                new_occ->rel_weight = occinfo->rel_weight;
                new_occ->nrel_weight = occinfo->nrel_weight;
                conwt++;
                occinfo++;
            }
            else {
                /* Info from old occ comes next */
                new_occ->con = occinfo->con;
                new_occ->ctype = occinfo->ctype;
                new_occ->rel_ret = occinfo->rel_ret;
                new_occ->nrel_ret = occinfo->nrel_ret;
                new_occ->nret = occinfo->nret;
                new_occ->rel_weight = occinfo->rel_weight;
                new_occ->nrel_weight = occinfo->nrel_weight;
                occinfo++;
            }
            new_occ++;
        }
        /* Finish off any query terms not included in occinfo*/
        while (conwt < end_conwt) {
            new_occ->con = conwt->con;
            new_occ->ctype = ctype;
            new_occ->query = 1;
            new_occ->orig_weight = conwt->wt;
            conwt++;
            new_occ++;
        }
    }
    
    /* Finish off any occinfo terms not included in query */
    while (occinfo < end_occinfo) {
        new_occ->con = occinfo->con;
        new_occ->ctype = occinfo->ctype;
        new_occ->rel_ret = occinfo->rel_ret;
        new_occ->nrel_ret = occinfo->nrel_ret;
        new_occ->nret = occinfo->nret;
        new_occ->rel_weight = occinfo->rel_weight;
        new_occ->nrel_weight = occinfo->nrel_weight;
        occinfo++;
        new_occ++;
    }

    fdbk_info->num_rel = fdbk_stats->num_rel;
    fdbk_info->num_occ = new_occ - fdbk_info->occ;

    return (1);
}
