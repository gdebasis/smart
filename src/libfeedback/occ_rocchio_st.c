#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfeedback/occ_rocchio_stats.c,v 11.2 1993/02/03 16:50:01 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "inv.h"
#include "tr_vec.h"
#include "vector.h"
#include "feedback.h"
#include "spec.h"
#include "trace.h"
#include "fdbk_stats.h"

/*
  Same as occ_rocchio, except that this program also writes out the 
  statistics gathered to a fdbkstats file.
*/

static char *fdbk_stats_file;
static long fdbk_stats_rwcmode;

static SPEC_PARAM spec_args[] = {
    {"feedback.fdbk_stats_file", getspec_dbfile, (char *) &fdbk_stats_file},
    {"feedback.fdbk_stats_file.rwcmode", getspec_filemode, (char *)&fdbk_stats_rwcmode},
    TRACE_PARAM ("feedback.occ_info.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int did_vec_inst;
static int fdbk_fd;


int
init_occ_rocchio_stats (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_occ_rocchio_stats");
    if (UNDEF == (did_vec_inst = init_did_vec (spec, (char *) NULL)))
        return (UNDEF);
    if (! VALID_FILE(fdbk_stats_file) ||
        UNDEF == (fdbk_fd = open_fdbkstats (fdbk_stats_file,
                                            fdbk_stats_rwcmode)))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_occ_rocchio_stats");
    return (1);
}


int
occ_rocchio_stats (unused, fdbk_info, inst)
char *unused;
FEEDBACK_INFO *fdbk_info;
int inst;
{
    long i, j, ctype;
    VEC dvec;
    CON_WT *conwt;
    long top_iter;
    OCC *occ_ptr, *end_occ_ptr;
    FDBK_STATS fdbk_stats;
    OCCINFO *occinfo;

    PRINT_TRACE (2, print_string, "Trace: entering occ_rocchio_stats");

    top_iter = -1;
    for (i = 0; i < fdbk_info->tr->num_tr; i++)
        if (fdbk_info->tr->tr[i].iter >= top_iter)
            top_iter = fdbk_info->tr->tr[i].iter;

    /* Find a retrieved nonrel doc in last iteration and add it */
    for (i = 0; i < fdbk_info->tr->num_tr; i++) {
        if (fdbk_info->tr->tr[i].rel == 0 &&
            fdbk_info->tr->tr[i].iter == top_iter) {
	    /* Get vector for the nonrel doc */
	    if (1 != did_vec (&fdbk_info->tr->tr[i].did, &dvec, did_vec_inst))
		return (UNDEF);

	    /* Go through dvec and fdbk_info->occ in parallel (both sorted by 
	       ctype,con) and add info for those in both to fdbk_info->occ */
	    occ_ptr = fdbk_info->occ;
	    end_occ_ptr = &fdbk_info->occ[fdbk_info->num_occ];
	    conwt = dvec.con_wtp;
	    for (ctype = 0; ctype < dvec.num_ctype; ctype++) {
		for (j = 0; j < dvec.ctype_len[ctype]; j++) {
		    while (occ_ptr < end_occ_ptr && occ_ptr->ctype < ctype)
			occ_ptr++;
		    while (occ_ptr < end_occ_ptr &&
			   occ_ptr->ctype == ctype &&
			   occ_ptr->con < conwt->con)
			occ_ptr++;
		    if (occ_ptr < end_occ_ptr &&
			occ_ptr->ctype == ctype &&
			occ_ptr->con == conwt->con) {
			occ_ptr->nrel_weight += conwt->wt;
			occ_ptr->nrel_ret++;
		    }
		    if (occ_ptr >= end_occ_ptr)
			break;
		    conwt++;
		}
	    }
	}
    }

    /* Fill in fdbk_stats with the information gathered, and write it out. */
    if (NULL == (fdbk_stats.occinfo = Malloc(fdbk_info->num_occ, OCCINFO)))
        return (UNDEF);
    occinfo = fdbk_stats.occinfo;

    for (i = 0; i < fdbk_info->num_occ; i++) { 
	occinfo->ctype = fdbk_info->occ[i].ctype;
        occinfo->con = fdbk_info->occ[i].con;
        occinfo->rel_ret = fdbk_info->occ[i].rel_ret;
        occinfo->nrel_ret = fdbk_info->occ[i].nrel_ret;
        occinfo->nret = fdbk_info->occ[i].nret;
        occinfo->rel_weight = fdbk_info->occ[i].rel_weight;
        occinfo->nrel_weight = fdbk_info->occ[i].nrel_weight;
        occinfo++;
    }

    fdbk_stats.id_num = fdbk_info->orig_query->id_num.id;
    fdbk_stats.num_ret = fdbk_info->tr->num_tr;
    fdbk_stats.num_rel = fdbk_info->num_rel;
    fdbk_stats.num_occinfo = fdbk_info->num_occ;

    if (UNDEF == seek_fdbkstats (fdbk_fd, &fdbk_stats) ||
        UNDEF == write_fdbkstats (fdbk_fd, &fdbk_stats))
        return (UNDEF);

    Free(fdbk_stats.occinfo);

    PRINT_TRACE (4, print_fdbk_info, fdbk_info);
    PRINT_TRACE (2, print_string, "Trace: leaving occ_rocchio_stats");

    return (1);
}


int
close_occ_rocchio_stats (inst)
int inst;
{

    PRINT_TRACE (2, print_string, "Trace: entering close_occ_rocchio_stats");

    if (UNDEF == close_did_vec (did_vec_inst))
        return (UNDEF);
    if (UNDEF == close_fdbkstats (fdbk_fd))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_occ_rocchio_stats");
    return (1);
}
