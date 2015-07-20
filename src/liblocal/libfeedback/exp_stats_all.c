#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libfeedback/exp_stats.c,v 11.0 1992/07/21 18:20:39 chrisb Exp $";
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
 *1 local.feedback.expand.exp_stats_all
 *2 exp_stats (unused1, fdbk_info, inst)
 *3   char *unused1;
 *3   FEEDBACK_INFO *fdbk_info;
 *3   int inst;
 *4 init_exp_stats_all (spec, unused)
 *5   "feedback.fdbk_stat_file"
 *5   "feedback.fdbk_stat_file.rmode"
 *5   "feedback.expand.trace"
 *5   "feedback.ctype.*.num_expand"
 *4 close_exp_stats_all (inst)
 *7 Same as exp_stats, except that no num_expand based selection occurs,
 *7 i.e. all terms are kept.
 *7 Occurrence statistics gathered from query dependent feedback_stats.
***********************************************************************/

static char *fdbk_stats_file;
static long fdbk_stats_rmode;

static SPEC_PARAM spec_args[] = {
    {"local.feedback.fdbk_stats_file",  getspec_dbfile, (char *) &fdbk_stats_file},
    {"local.feedback.fdbk_stats_file.rmode",getspec_filemode,(char *)&fdbk_stats_rmode},
    TRACE_PARAM ("feedback.expand.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static int fdbk_fd;

static int merge_query();


int
init_exp_stats_all (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_exp_stats_all");

    if (! VALID_FILE(fdbk_stats_file) ||
        UNDEF == (fdbk_fd = open_fdbkstats (fdbk_stats_file,
                                            fdbk_stats_rmode)))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_exp_stats_all");
    return (1);
}

int
exp_stats_all (unused, fdbk_info, inst)
char *unused;
FEEDBACK_INFO *fdbk_info;
int inst;
{
    FDBK_STATS fdbk_stats;
    int status;

    PRINT_TRACE (2, print_string, "Trace: entering exp_stats_all");

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

    PRINT_TRACE (4, print_fdbk_info, fdbk_info);
    PRINT_TRACE (2, print_string, "Trace: leaving exp_stats_all");
    return (1);
}

int
close_exp_stats_all (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_exp_stats_all");

    if (UNDEF == close_fdbkstats (fdbk_fd))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_exp_stats_all");
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
