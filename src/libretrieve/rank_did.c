#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libretrieve/rank_trec_did.c,v 11.2 1993/02/03 16:52:53 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Keep track of top ranked documents during course of a retrieval.
 *1 retrieve.rank_tr.rank_did
 *2 rank_did (new, results, inst)
 *3   TOP_RESULT *new;
 *3   RESULT *results;
 *3   int inst;
 *7 Add the new document to results.top_results if the similarity is
 *7 high enough.  Break ties among similarities by ranking the document
 *7 with highest docid first.
 *7 Assumes upon new query calling routine sets results->min_top_sim to 0.0 
 *7 and zero's out the top_results array.
 *7 Assumes sims in top_sims only increase (positive weights).
***********************************************************************/


#include "common.h"
#include "param.h"
#include "proc.h"
#include "functions.h"
#include "retrieve.h"

static long min_sim_index;

int
rank_did (new, results, inst)
TOP_RESULT *new;
RESULT *results;
int inst;
{
    long num_wanted = results->num_top_results;

    TOP_RESULT *last_res = &(results->top_results[num_wanted]);

    TOP_RESULT *min_res;   /* Position of doc with current min sim */
    TOP_RESULT *new_min_res; /* Position of doc with new min sim */
    TOP_RESULT *temp_res;
    TOP_RESULT *save_res;
    TOP_RESULT save_min;   /* Saved copy of min_res */

#ifdef DEBUGRANK
    if (new->did == 1378) {
        print_long (&new->did, NULL);
        print_top_results (results, NULL);
    }
#endif

    if (num_wanted <= 0)
        return (0);

    /* Reset min_sim_index if new query */
    if (results->min_top_sim == 0.0 && results->top_results[0].did == 0)
        min_sim_index = 0;

    min_res = &(results->top_results[min_sim_index]);


#ifdef DEBUGRANK
    if (min_res->did == 1378) {
        print_string ("1378 is min_res", NULL);
    }
#endif

    /* Enter doc into top_results if similarity high enough. */
    if (new->sim < min_res->sim ||
        (new->sim == min_res->sim && 
         new->did <= min_res->did))
        /* Don't need to enter */
        return (0);

    save_min.did = min_res->did;
    save_min.sim = min_res->sim;

    min_res->sim = new->sim;
    min_res->did = new->did;

    new_min_res = results->top_results;
    save_res = (TOP_RESULT *) NULL;
    for (temp_res = results->top_results;
         temp_res < last_res;
         temp_res++) {
        if (new->did == temp_res->did && temp_res != min_res)
            save_res = temp_res;
        if (temp_res->sim < new_min_res->sim ||
            (temp_res->sim == new_min_res->sim &&
             temp_res->did < new_min_res->did)) {
            new_min_res = temp_res;
        }
    }

    if (save_res != (TOP_RESULT *) NULL) {
        /* New doc was already in top docs and now is represented twice */
        /* Restore old min_res in old location of new doc */
        /* Assumes new similarity for new doc is greater than old */
        /* similarity */
        save_res->sim = save_min.sim;
        save_res->did = save_min.did;
        if (save_res->sim < new_min_res->sim ||
            (save_res->sim == new_min_res->sim &&
             save_res->did < new_min_res->did)) {
            new_min_res = save_res;
        }
    }

    /* print_top_results (results, NULL); */
    results->min_top_sim = new_min_res->sim;
    min_sim_index = new_min_res - results->top_results;


#ifdef DEBUGRANK
    if (new->did == 1378) {
        print_long (&new->did, NULL);
        print_top_results (results, NULL);
    }
#endif 

    return (1);
}
