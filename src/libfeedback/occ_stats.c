#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/./src/libfeedback/occ_inv.c,v 11.0 92/07/14 11:07:05 smart Exp Locker: smart $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Feedback occurrence info procedure - make feedback statistical file.
 *1 feedback.occ_info.collstats_all_stats
 *2 occ_collstats_all_stats (unused1, fdbk_info, inst)
 *3   char *unused1;
 *3   FEEDBACK_INFO *fdbk_info;
 *3   int inst;
 *4 init_occ_collstats_all_stats (spec, unused)
 *5   "feedback.fdbk_stats_file"
 *5   "feedback.fdbk_stats_file.rwmode"
 *5   "feedback.num_ctypes"
 *5   "feedback.ctype.*.collstat_file"
 *5   "feedback.ctype.*.collstat_file.rmode"
 *5   "feedback.occ_info.trace"
 *4 close_occ_collstats_all_stats (inst)
 *7 Same as occ_inv except sum weights among all non-relevent docs
 *7  instead of just retrieved non-rel docs. Info about sum
 *7 gotten from COLLSTATS_TOTWT array of collstats file.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "tr_vec.h"
#include "vector.h"
#include "feedback.h"
#include "spec.h"
#include "trace.h"
#include "fdbk_stats.h"
#include "dir_array.h"
#include "collstat.h"

static char *fdbk_stats_file;
static long fdbk_stats_rwcmode;
static long num_ctypes;

static SPEC_PARAM spec_args[] = {
    {"feedback.fdbk_stats_file",  getspec_dbfile, (char *) &fdbk_stats_file},
    {"feedback.fdbk_stats_file.rwcmode",getspec_filemode,(char *)&fdbk_stats_rwcmode},
    {"feedback.num_ctypes",        getspec_long, (char *) &num_ctypes},
    TRACE_PARAM ("feedback.occ_info.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static char *collstat_file;           
static long collstat_file_mode;

static char *prefix;
static SPEC_PARAM_PREFIX prefix_spec_args[] = {
    {&prefix, "collstat_file",         getspec_dbfile, (char *) &collstat_file},
    {&prefix, "collstat_file.rmode",   getspec_filemode, (char *) &collstat_file_mode},
    };
static int num_prefix_spec_args =
    sizeof (prefix_spec_args) / sizeof (prefix_spec_args[0]);

static struct coll_info {
    float *listweight;
    long num_listweight;
    long *freq;
    long num_freq;
} *collstats_info;
static int fdbk_fd;

int
init_occ_collstats_all_stats (spec, unused)
SPEC *spec;
char *unused;
{
    char param_prefix[40];
    long i;
    int collstats_fd;
    DIR_ARRAY dir_array;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_occ_collstats_all_stats");

    if (! VALID_FILE(fdbk_stats_file) ||
        UNDEF == (fdbk_fd = open_fdbkstats (fdbk_stats_file,
                                            fdbk_stats_rwcmode)))
        return (UNDEF);

    /* Reserve space for collection info about total collection weights */
    if (NULL == (collstats_info = (struct coll_info *)
                 malloc ((unsigned) num_ctypes * sizeof (struct coll_info)))) {
        return (UNDEF);
    }

    prefix = param_prefix;
    for (i = 0; i < num_ctypes; i++) {
        (void) sprintf (param_prefix, "feedback.ctype.%ld.", i);
        if (UNDEF == lookup_spec_prefix (spec,
                                         &prefix_spec_args[0],
                                         num_prefix_spec_args))
            return (UNDEF);
        /* Note there may not be a valid collstat file for all ctypes;
           may be OK as long as careful about feedback for that ctype */
        if (! VALID_FILE (collstat_file)) {
            collstats_info[i].listweight = (float *) NULL;
            collstats_info[i].num_listweight = 0;
        }
        else {
            if (UNDEF == (collstats_fd = open_dir_array (collstat_file,
                                                         collstat_file_mode)))
                return (UNDEF);
            dir_array.id_num = COLLSTAT_TOTWT;
            if (1 != seek_dir_array (collstats_fd, &dir_array) ||
                1 != read_dir_array (collstats_fd, &dir_array)) {
                collstats_info[i].listweight = (float *) NULL;
                collstats_info[i].num_listweight = 0;
            }
            else {
                if (NULL == (collstats_info[i].listweight = (float *)
                             malloc ((unsigned) dir_array.num_list)))
                    return (UNDEF);
                (void) bcopy (dir_array.list,
                              (char *) collstats_info[i].listweight,
                              dir_array.num_list);
                collstats_info[i].num_listweight = dir_array.num_list / 
                    sizeof (float);
            }

            dir_array.id_num = COLLSTAT_COLLFREQ;
            if (1 != seek_dir_array (collstats_fd, &dir_array) ||
                1 != read_dir_array (collstats_fd, &dir_array)) {
                collstats_info[i].freq = (long *) NULL;
                collstats_info[i].num_freq = 0;
            }
            else {
                if (NULL == (collstats_info[i].freq = (long *)
                             malloc ((unsigned) dir_array.num_list)))
                    return (UNDEF);
                (void) bcopy (dir_array.list,
                              (char *) collstats_info[i].freq,
                              dir_array.num_list);
                collstats_info[i].num_freq = dir_array.num_list / 
                    sizeof (long);
            }
            if (UNDEF == close_dir_array (collstats_fd))
                return (UNDEF);
        }
    }

    PRINT_TRACE (2, print_string, "Trace: leaving init_occ_collstats_all_stats");
    return (1);
}

int
occ_collstats_all_stats (unused, fdbk_info, inst)
char *unused;
FEEDBACK_INFO *fdbk_info;
int inst;
{
    long i;
    FDBK_STATS fdbk_stats;
    OCCINFO *occinfo;
    long ctype;
    long con;

    PRINT_TRACE (2, print_string, "Trace: entering occ_collstats_all_stats");
    PRINT_TRACE (6, print_fdbk_info, fdbk_info);

    if (NULL == (occinfo = (OCCINFO *)
                 malloc ((unsigned) fdbk_info->num_occ * sizeof (OCCINFO))))
        return (UNDEF);
    fdbk_stats.occinfo = occinfo;

    for (i = 0; i < fdbk_info->num_occ; i++) {
        ctype =fdbk_info->occ[i].ctype; 
        con = fdbk_info->occ[i].con;
        occinfo->ctype = ctype;
        occinfo->con = con;
        occinfo->rel_ret = fdbk_info->occ[i].rel_ret;
        occinfo->rel_weight = fdbk_info->occ[i].rel_weight;
        if (collstats_info[ctype].freq == NULL ||
            collstats_info[ctype].num_freq <= con ||
            collstats_info[ctype].listweight == NULL ||
            collstats_info[ctype].num_listweight <= con) {
            occinfo->nrel_ret = 0;
            occinfo->nrel_weight = 0;
        }
        else {   
            occinfo->nrel_ret = collstats_info[ctype].freq[con] - 
                fdbk_info->occ[i].rel_ret;
            occinfo->nrel_weight = collstats_info[ctype].listweight[con] - 
                fdbk_info->occ[i].rel_weight;
        }
        occinfo->nret = 0;
        occinfo++;
    }

    fdbk_stats.id_num = fdbk_info->orig_query->id_num.id;
    fdbk_stats.num_ret = fdbk_info->tr->num_tr;
    fdbk_stats.num_rel = fdbk_info->num_rel;
    fdbk_stats.num_occinfo = fdbk_info->num_occ;

    if (UNDEF == seek_fdbkstats (fdbk_fd, &fdbk_stats) ||
        UNDEF == write_fdbkstats (fdbk_fd, &fdbk_stats))
        return (UNDEF);

/*     PRINT_TRACE (4, print_fdbk_stats, fdbk_stats); */
    PRINT_TRACE (2, print_string, "Trace: leaving occ_collstats_all_stats");

    return (1);
}


int
close_occ_collstats_all_stats (inst)
int inst;
{
    long i;

    PRINT_TRACE (2, print_string, "Trace: entering close_occ_collstats_all_stats");

    if (UNDEF == close_fdbkstats (fdbk_fd))
        return (UNDEF);

    for (i = 0; i < num_ctypes; i++) {
        if (NULL != collstats_info[i].freq)
            (void) free ((char *)collstats_info[i].freq);
        if (NULL != collstats_info[i].listweight)
            (void) free ((char *)collstats_info[i].listweight);
    }
    (void) free ((char *) collstats_info);

    PRINT_TRACE (2, print_string, "Trace: leaving close_occ_collstats_all_stats");
    return (1);
}
