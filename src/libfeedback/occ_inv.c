#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/./src/libfeedback/occ_inv.c,v 11.0 92/07/14 11:07:05 smart Exp Locker: smart $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/


#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "inv.h"
#include "tr_vec.h"
#include "vector.h"
#include "feedback.h"
#include "spec.h"
#include "trace.h"

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Feedback occurrence info procedure - inv statistics from all nonrel docs
 *1 feedback.occ_info.inv_all
 *2 occ_inv (unused1, fdbk_info, inst)
 *3   char *unused1;
 *3   FEEDBACK_INFO *fdbk_info;
 *3   int inst;
 *4 init_occ_inv (spec, unused)
 *5   "feedback.num_ctypes"
 *5   "feedback.ctype.*.inv_file"
 *5   "feedback.ctype.*.inv_file.rmode"
 *5   "feedback.occ_info.trace"
 *4 close_occ_inv (inst)
 *7 Same as occ_inv except sum weights among all non-relevent docs
 *7  instead of just retrieved non-rel docs. 
***********************************************************************/

static long num_ctypes;

static SPEC_PARAM spec_args[] = {
    {"feedback.num_ctypes",        getspec_long, (char *) &num_ctypes},
    TRACE_PARAM ("feedback.occ_info.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

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

int
init_occ_inv (spec, unused)
SPEC *spec;
char *unused;
{
    char param_prefix[40];
    long i;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_occ_inv");

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

    PRINT_TRACE (2, print_string, "Trace: leaving init_occ_inv");
    return (1);
}

int
occ_inv (unused, fdbk_info, inst)
char *unused;
FEEDBACK_INFO *fdbk_info;
int inst;
{
    long i, k;
    long tr_ind;
    INV inv;

    PRINT_TRACE (2, print_string, "Trace: entering occ_inv");

    for (i = 0; i < fdbk_info->num_occ; i++) {
        inv.id_num = fdbk_info->occ[i].con;
        if (fdbk_info->occ[i].ctype >= num_ctypes) {
            set_error (SM_INCON_ERR, "Ctype too large", "occ_inv");
            return (UNDEF);
        }
        if (1 != seek_inv (inv_fd[fdbk_info->occ[i].ctype], &inv) ||
            1 != read_inv (inv_fd[fdbk_info->occ[i].ctype], &inv) ) {
            PRINT_TRACE (1, print_string, "occ_info: concept not found in inv_file:");
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
                    /* Nonrel occurrence (rel occurrences already handled
                       by expand routines) */
                    fdbk_info->occ[i].nrel_ret++;
                    fdbk_info->occ[i].nrel_weight += inv.listwt[k].wt;
                }
            }
            else {
                fdbk_info->occ[i].nret++;
            }
        }
    }

    PRINT_TRACE (4, print_fdbk_info, fdbk_info);
    PRINT_TRACE (2, print_string, "Trace: leaving occ_inv");

    return (1);
}


int
close_occ_inv (inst)
int inst;
{
    long i;

    PRINT_TRACE (2, print_string, "Trace: entering close_occ_inv");

    for (i = 0; i < num_ctypes; i++) {
        if (UNDEF == close_inv (inv_fd[i]))
            return (UNDEF);
        inv_fd[i] = UNDEF;
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_occ_inv");
    return (1);
}
