#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/./src/libfeedback/wt_ide.c,v 11.0 92/07/14 11:07:03 smart Exp Locker: smart $";
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
#include "feedback.h"
#include "smart_error.h"
#include "rel_header.h"

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Low-level feedback weighting procedure - NClassical Probabilistic
 *1 feedback.weight.nclass
 *2 wt_prc_nclass (unused1, fdbk_info, inst)
 *3   char *unused1;
 *3   FEEDBACK_INFO *fdbk_info;
 *3   int inst;
 *4 init_wt_prc_nclass (spec, unused)
 *5   "feedback.doc.tectloc_file"
 *5   "feedback.weight.trace"
 *4 close_wt_prc_nclass (inst)
 *7
 *7 compute the feedback weight for each individual query term i according
 *7 to the formula
 *7          pi (1-qi)
 *7   log ( ---------- )
 *7          qi (1-pi)
 *7
 *7  where        rel_ret + freq/num_docs
 *7          pi = ----------------------
 *7                  num_rel + 1.0
 *7
 *7               freq - rel_ret + freq/num_docs
 *7          qi = ---------------------------
 *7                 num_doc - num_rel + 1.0
 *7  freq is the frequency of term i in the entire collection.
 *7  rel_ret is the number of relevant docs term i is in
 *7  num_rel is the number of relevant docs.
 *7  num_doc is the number of docs.
 *7
 *7  fdbk_info gives all occurrence information for each term, as well as 
 *7  storing the resulting weight.
 *7
 *7 nclass differs from class only in additive term that gets probablity
 *7 away from 0.0 and 1.0.  Nclass approach works better than class for
 *7 relevance feedback with low numbers of relevant docs.
***********************************************************************/

static char *textloc_file;

static SPEC_PARAM spec_args[] = {
    {"feedback.doc.textloc_file", getspec_dbfile, (char *) &textloc_file},
    TRACE_PARAM ("feedback.weight.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);


static long num_docs = 0;

int
init_wt_prc_nclass (spec, unused)
SPEC *spec;
char *unused;
{
    REL_HEADER *rh;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_wt_prc_nclass");

    if (NULL != (rh = get_rel_header (textloc_file)) && rh->num_entries > 0)
        num_docs = rh->num_entries;
    else {
        set_error (SM_INCON_ERR, textloc_file, "init_wt_prc_nclass");
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving init_wt_prc_nclass");
    return (1);
}

int
wt_prc_nclass (unused, fdbk_info, inst)
char *unused;
FEEDBACK_INFO *fdbk_info;
int inst;
{
    long i;
    double pi, qi;
    double freqN;

    PRINT_TRACE (2, print_string, "Trace: entering wt_prc_nclass");

    for (i = 0; i < fdbk_info->num_occ; i++) {
        freqN = (fdbk_info->occ[i].rel_ret + fdbk_info->occ[i].nrel_ret +
                 fdbk_info->occ[i].nret)
                 / (double) num_docs;
        pi = (fdbk_info->occ[i].rel_ret + freqN) 
            / (fdbk_info->num_rel + 1.0);
        qi = (fdbk_info->occ[i].nrel_ret + fdbk_info->occ[i].nret + freqN)
            / (num_docs - fdbk_info->num_rel + 1.0);
        fdbk_info->occ[i].weight = log ((pi * (1.0 - qi)) /
                                        (qi * (1.0 - pi)));
        /*          if (fdbk_info->occ[i].weight < 0.0)
         *                fdbk_info->occ[i].weight = 0.0;
         */
    }

    PRINT_TRACE (4, print_fdbk_info, fdbk_info);
    PRINT_TRACE (2, print_string, "Trace: leaving wt_prc_nclass");

    return (1);
}

int
close_wt_prc_nclass (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering/leaving close_wt_prc_nclass");
    return (0);
}
