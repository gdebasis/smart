#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/res_tr.c,v 11.2 1993/02/03 16:49:22 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Convert retrieval results into a tr_vec form
 *2 res_tr (results, tr_vec)
 *3   RESULT *results;
 *3   TR_VEC *tr_vec;
 *7 The list of top retrieved docs for a single query, found in results,
 *7 is added to tr_vec.  If tr_vec is non-empty (tr_vec->num_tr > 0), the
 *7 previous results are kept, and the current results are added with an
 *7 iteration number one more than previously used.  Tr_vec is kept sorted
 *7 by docid.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "retrieve.h"
#include "tr_vec.h"

static int compare_tr_did();
static TR_TUP *tr_tup_buf = (TR_TUP *) NULL;
static int size_tr_tup_buf = 0;

/* Add results in proper format to tr_vec */
/* tr_vec upon input has info from any previous iteration of this query */
/* Memory management is tricky here (output parameter of res_tr can actually
   have some incoming valid data which has been malloced elsewhere)
   Code is conservative which means we will probably leak some memory */

int
init_res_tr (unused1, unused2)
SPEC *unused1;
char *unused2;
{
    return (0);
}

int
res_tr (results, tr_vec, store_nsim)
RESULT *results;
TR_VEC *tr_vec;
int store_nsim;
{
    register TOP_RESULT *new_res;
    int next_iter;
    register TR_TUP *tr_tup;
    long rank;
    long i;

    if (results == NULL || tr_vec == NULL) 
        return (UNDEF);

    tr_vec->qid = results->qid;

    next_iter = 0;
    for (i = 0; i < tr_vec->num_tr; i++) {
        /* Set next_iter to indicate */
        /* current top docs came from new iteration of feedback */
        if (next_iter <= tr_vec->tr[i].iter) {
            next_iter = tr_vec->tr[i].iter + 1;
        }
    }

    /* Reserve more space if needed.  Copy old results into current vector */
    if (tr_vec->num_tr + results->num_top_results > size_tr_tup_buf) {
        size_tr_tup_buf = tr_vec->num_tr + results->num_top_results + 15;
	if (tr_vec->num_tr > 0 &&
	    tr_vec->tr == tr_tup_buf &&
	    tr_tup_buf != (TR_TUP *) NULL) {
	    if (NULL == (tr_tup_buf = (TR_TUP *)
			 realloc ((char *) tr_tup_buf,
				  (unsigned) size_tr_tup_buf*sizeof (TR_TUP))))
		return (UNDEF);
	    tr_vec->tr = tr_tup_buf;
	}
        else {
	    if (NULL == (tr_tup_buf = (TR_TUP *)
                     malloc ((unsigned) size_tr_tup_buf * sizeof (TR_TUP))))
            return (UNDEF);
	}
    }
    if (tr_vec->num_tr > 0 && tr_vec->tr != tr_tup_buf)
        bcopy ((char *) tr_vec->tr,
               (char *) tr_tup_buf,
               (int) tr_vec->num_tr * sizeof (TR_TUP));

    tr_vec->tr = tr_tup_buf;

    /* Top_results are kept in decreasing sim order.  Add rank */
    rank = 1;
    tr_tup = &tr_vec->tr[tr_vec->num_tr];
    for (new_res = results->top_results;
         new_res < &(results->top_results[results->num_top_results]); new_res++) {
		if (!store_nsim && new_res->sim <= 0.0)
			continue;  // Don't store negative similarities unless explicitly told to do so
 		if (new_res->sim <= results->min_top_sim)
			continue;
        tr_tup->did = new_res->did;
        tr_tup->rank = rank++;
        tr_tup->action = '\0';
        tr_tup->rel = 0;
        tr_tup->iter = next_iter;
        tr_tup->trtup_unused = '\0';
        tr_tup->sim = new_res->sim;
        tr_tup++;
        tr_vec->num_tr++;
    }

    /* sort tr_vec by did */
    qsort ((char *) tr_vec->tr,
           (int) tr_vec->num_tr,
           sizeof (TR_TUP),
           compare_tr_did);

    return (1);
}

int
close_res_tr (unused)
int unused;
{
    return (0);
}

static int
compare_tr_did (ptr1, ptr2)
TR_TUP *ptr1;
TR_TUP *ptr2;
{
    if (ptr1->did < ptr2->did)
        return (-1);
    if (ptr1->did > ptr2->did)
        return (1);
    return (0);
}
