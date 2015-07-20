#ifdef RCSID
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "fdbk_stats.h"
#include "feedback.h"
#include "functions.h"
#include "inv.h"
#include "param.h"
#include "smart_error.h"
#include "spec.h"
#include "tr_vec.h"
#include "trace.h"
#include "vector.h"

/********************   PROCEDURE DESCRIPTION   ************************
 * Quick and dirty hack to compute stats for rel-docs, merge it with
 * existing stats for non-rel docs. and write the merged stats out to
 * a file.
 * Based on exp_stats_all and occ_stats.
***********************************************************************/

static char *in_fs_file, *out_fs_file;
static long fs_rmode, fs_rwmode;
static long num_ctypes;
static SPEC_PARAM spec_args[] = {
    {"local.feedback.in_fs_file", getspec_dbfile, (char *) &in_fs_file},
    {"local.feedback.out_fs_file", getspec_dbfile, (char *) &out_fs_file},
    {"local.feedback.fs_file.rmode", getspec_filemode, (char *)&fs_rmode},
    {"local.feedback.fs_file.rwmode", getspec_filemode, (char *)&fs_rwmode},
    {"local.feedback.num_ctypes", getspec_long, (char *) &num_ctypes},
    TRACE_PARAM ("feedback.expand.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static char *inv_file;           
static long inv_file_mode;
static char *prefix;
static SPEC_PARAM_PREFIX prefix_spec_args[] = {
    {&prefix, "inv_file",         getspec_dbfile, (char *) &inv_file},
    {&prefix, "inv_file.rmode",   getspec_filemode, (char *) &inv_file_mode},
};
static int num_prefix_spec_args =
	sizeof (prefix_spec_args) / sizeof (prefix_spec_args[0]);

static int *inv_fd;
static int in_fdbk_fd, out_fdbk_fd;


int
init_occ_stats_stats (spec, unused)
SPEC *spec;
char *unused;
{
    char param_prefix[40];
    long i;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_occ_stats_stats");

    if (! VALID_FILE(in_fs_file) ||
	! VALID_FILE(out_fs_file) ||
        UNDEF == (in_fdbk_fd = open_fdbkstats(in_fs_file, fs_rmode)) ||
	UNDEF == (out_fdbk_fd = open_fdbkstats(out_fs_file, fs_rwmode)))
        return (UNDEF);
    /* Reserve space for inverted file descriptors */
    if (NULL == (inv_fd = (int *)
                 malloc ((unsigned) num_ctypes * sizeof (int)))) {
        return (UNDEF);
    }

    prefix = param_prefix;
    for (i = 0; i < num_ctypes; i++) {
        (void) sprintf (param_prefix, "feedback.ctype.%ld.", i);
        if (UNDEF == lookup_spec_prefix (spec,
                                         &prefix_spec_args[0],
                                         num_prefix_spec_args))
            return (UNDEF);
        /* Note there may not be a valid inverted file for all ctypes;
           may be OK as long as careful about feedback for that ctype */
        if (! VALID_FILE (inv_file))
            inv_fd[i] = UNDEF;
        else {
            if (UNDEF == (inv_fd[i] = open_inv (inv_file, inv_file_mode)))
                return (UNDEF);
        }
    }

    PRINT_TRACE (2, print_string, "Trace: leaving init_occ_stats_stats");
    return (1);
}

int
occ_stats_stats (unused, fdbk_info, inst)
char *unused;
FEEDBACK_INFO *fdbk_info;
int inst;
{
    int status;
    long tr_ind, i, j, k;
    FDBK_STATS fdbk_stats, merged_stats;
    OCCINFO *occinfo, *old_occinfo;
    INV inv;

    PRINT_TRACE (2, print_string, "Trace: entering occ_stats_stats");

    /* read previous info for relevance occurrences for this query */
    /* if no info, set fdbk_stats.id_num to UNDEF */
    fdbk_stats.id_num = fdbk_info->orig_query->id_num.id;
    if (1 != (status = seek_fdbkstats (in_fdbk_fd, &fdbk_stats))) {
        if (status == UNDEF)
            return (UNDEF);
        fdbk_stats.id_num = UNDEF;
    }
    else {
        if (1 != (status = read_fdbkstats (in_fdbk_fd, &fdbk_stats))) {
            if (status == UNDEF)
                return (UNDEF);
            fdbk_stats.id_num = UNDEF;
        }
    }

    old_occinfo = fdbk_stats.occinfo;
    if (NULL == (merged_stats.occinfo = Malloc(fdbk_info->num_occ, OCCINFO)))
        return (UNDEF);
    occinfo = merged_stats.occinfo;

    for (i = 0; i < fdbk_info->num_occ; i++) {
	/* Try getting nrel info from old stats */
	for (j = 0; j < fdbk_stats.num_occinfo; j++) {
	    if (old_occinfo[j].ctype == fdbk_info->occ[i].ctype &&
		old_occinfo[j].con == fdbk_info->occ[i].con) {
		fdbk_info->occ[i].nrel_ret = old_occinfo[j].nrel_ret;
		fdbk_info->occ[i].nret = old_occinfo[j].nret +
		    old_occinfo[j].rel_ret - fdbk_info->occ[i].rel_ret;
		fdbk_info->occ[i].nrel_weight = old_occinfo[j].nrel_weight;
		break;
	    }   
	}
	if (j == fdbk_stats.num_occinfo) {
	    /* Not found in old stats; fill in from inv file */
	    inv.id_num = fdbk_info->occ[i].con;
	    if (fdbk_info->occ[i].ctype >= num_ctypes) {
		set_error (SM_INCON_ERR, "Ctype too large", "occ_inv_stats");
		return (UNDEF);
	    }
	    if (UNDEF != inv_fd[fdbk_info->occ[i].ctype]) {
		if (1 != seek_inv (inv_fd[fdbk_info->occ[i].ctype], &inv) ||
		    1 != read_inv (inv_fd[fdbk_info->occ[i].ctype], &inv)) {
                    PRINT_TRACE (1, print_string, "exp_corr: concept not found in inv_file:");
                    PRINT_TRACE (1, print_long, &fdbk_info->occ[i].con);
		    continue;
		}

		tr_ind = 0;
		for (k = 0; k < inv.num_listwt; k++) {
		    /* Discover whether doc is in top_docs */
		    /* Assume both inverted list and tr are sorted */
		    while (tr_ind < fdbk_info->tr->num_tr &&
			   fdbk_info->tr->tr[tr_ind].did < inv.listwt[k].list)
			tr_ind++;

		    if (tr_ind < fdbk_info->tr->num_tr &&
			fdbk_info->tr->tr[tr_ind].did == inv.listwt[k].list) {
			if (! fdbk_info->tr->tr[tr_ind].rel) {
			    fdbk_info->occ[i].nrel_ret++;
			    fdbk_info->occ[i].nrel_weight += inv.listwt[k].wt;
			}
		    }
		    else {
			fdbk_info->occ[i].nret++;
		    }
		}
	    }
	}

	occinfo->ctype = fdbk_info->occ[i].ctype;
	occinfo->con = fdbk_info->occ[i].con;
	occinfo->rel_ret = fdbk_info->occ[i].rel_ret;
	occinfo->nrel_ret = fdbk_info->occ[i].nrel_ret;
	occinfo->nret = fdbk_info->occ[i].nret;
	occinfo->rel_weight = fdbk_info->occ[i].rel_weight;
	occinfo->nrel_weight = fdbk_info->occ[i].nrel_weight;
	occinfo++;
    }

    merged_stats.id_num = fdbk_info->orig_query->id_num.id;
    merged_stats.num_ret = fdbk_info->tr->num_tr;
    merged_stats.num_rel = fdbk_info->num_rel;
    merged_stats.num_occinfo = occinfo - merged_stats.occinfo;

    if (UNDEF == seek_fdbkstats(out_fdbk_fd, &merged_stats) ||
        UNDEF == write_fdbkstats(out_fdbk_fd, &merged_stats))
        return (UNDEF);

    Free(merged_stats.occinfo);

    PRINT_TRACE (4, print_fdbk_info, fdbk_info);
    PRINT_TRACE (2, print_string, "Trace: leaving occ_stats_stats");
    return (1);
}

int
close_occ_stats_stats (inst)
int inst;
{
    long i;

    PRINT_TRACE (2, print_string, "Trace: entering close_occ_stats_stats");

    if (UNDEF == close_fdbkstats(in_fdbk_fd) ||
	UNDEF == close_fdbkstats(out_fdbk_fd))
        return (UNDEF);

    for (i = 0; i < num_ctypes; i++) {
        if (UNDEF != inv_fd[i] &&
            UNDEF == close_inv(inv_fd[i]))
            return (UNDEF);
        inv_fd[i] = UNDEF;
    }

   PRINT_TRACE (2, print_string, "Trace: leaving close_occ_stats_stats");
   return (0);
}
