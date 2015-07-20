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
 *0 Low-level feedback weighting procedure - Reverse Probabilistic
 *1 feedback.weight.rev
 *2 wt_prc_rev (unused1, fdbk_info, inst)
 *3   char *unused1;
 *3   FEEDBACK_INFO *fdbk_info;
 *3   int inst;
 *4 init_wt_prc_rev (spec, unused)
 *5   "feedback.doc.tectloc_file"
 *5   "feedback.weight.trace"
 *4 close_wt_prc_rev (inst)
 *7
 *7 compute the feedback weight for an individual query term i according
 *7 to the "reverse" formula
 *7          pi (1-qi)
 *7   log ( ---------- )
 *7          qi (1-pi)
 *7
 *7  where          rel_reti + num_rel/num_doc
 *7          pi = -----------------------------                   (P(REL|Xi=1))
 *7                rel_reti + nonrel_reti + 1.0
 *7
 *7                num_rel - rel_reti + num_rel/num_doc
 *7          qi = --------------------------------------          (P(REL|Xi=0))
 *7                num_ret - rel_reti - nonrel_reti + 1.0
 *7
 *7  rel_reti is the number of relevant retrieved docs term i is in
 *7  nonrel_reti is the number of nonrelevant retrieved docs containing term i
 *7  num_rel is the number of relevant docs.
 *7  num_ret is the number of retrieved documents.
 *7  num_doc is the number of docs.
 *7

 *7  fdbk_info gives all occurrence information for each term, as well as 
 *7  storing the resulting weight.
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
init_wt_prc_rev (spec, unused)
SPEC *spec;
char *unused;
{
    REL_HEADER *rh;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_wt_prc_rev");

    if (NULL != (rh = get_rel_header (textloc_file)) && rh->num_entries > 0)
        num_docs = rh->num_entries;
    else {
        set_error (SM_INCON_ERR, textloc_file, "init_wt_prc_rev");
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving init_wt_prc_rev");
    return (1);
}

int
wt_prc_rev (unused, fdbk_info, inst)
char *unused;
FEEDBACK_INFO *fdbk_info;
int inst;
{
    long i;
    double pi, qi;

    PRINT_TRACE (2, print_string, "Trace: entering wt_prc_rev");

    for (i = 0; i < fdbk_info->num_occ; i++) {
        pi = ((double) fdbk_info->occ[i].rel_ret + 
              (double) fdbk_info->num_rel / num_docs)
            / (fdbk_info->occ[i].rel_ret + fdbk_info->occ[i].nrel_ret + 1.0);
        qi = ((double)fdbk_info->num_rel - fdbk_info->occ[i].rel_ret + 
              (double) fdbk_info->num_rel / num_docs)
            / (double) (fdbk_info->tr->num_tr - fdbk_info->occ[i].rel_ret 
               - fdbk_info->occ[i].nrel_ret + 1.0);
        fdbk_info->occ[i].weight = log ((pi * (1.0 - qi)) /
                                        (qi * (1.0 - pi)));

        if (fdbk_info->occ[i].weight < 0.0)
            fdbk_info->occ[i].weight = 0.0;
    }
    
    PRINT_TRACE (4, print_fdbk_info, fdbk_info);
    PRINT_TRACE (2, print_string, "Trace: leaving wt_prc_rev");

    return (1);
}

int
close_wt_prc_rev (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering/leaving close_wt_prc_rev");
    return (0);
}
