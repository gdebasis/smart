#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libretrieve/trec_vec_inv.c,v 11.2 1993/02/03 16:52:56 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Match whole query versus doc collection using inverted search
 *1 retrieve.coll_sim.vec_inv
 *1 retrieve.coll_sim.inverted
 *2 sim_vec_inv_lm_nsim (in_retrieval, results, inst)
 *3   RETRIEVAL *in_retrieval;
 *3   RESULT *results;
 *3   int inst;
 *4 init_sim_vec_inv_lm_nsim (spec, unused)
 *5   "retrieve.num_wanted"
 *5   "retrieve.textloc_file"
 *5   "index.ctype.*.inv_sim"
 *5   "retrieve.coll_sim.trace"
 *4 close_sim_vec_inv_lm_nsim (inst)

 *7 Take a vector query given by in_retrieval, and match it against every
 *7 document in doc_file in turn, calling a ctype dependant matching
 *7 function given by the procedure index.ctype.N.inv_sim for ctype N.
 *7 The similarity of all docs to the query is returned in
 *7 results->full_results.  The top similarities are kept track of by
 *7 inv_sim and are returned in results->top_results.
 *7 Return UNDEF if error, else 1.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "context.h"
#include "retrieve.h"
#include "rel_header.h"
#include "vector.h"
#include "tr_vec.h"
#include "docdesc.h"
#include "inst.h"

/*  Perform inner product similarity function using inverted file to access */
/*  document weights.  Query weights given by query vector.  */

/* Basic algorithm: malloc enough space to keep track of all similarities */
/*  of documents with direct access by did. */
/*  Top num_wanted similarites kept track of throughout. */


static int isAugmented ;

static SPEC_PARAM spec_args[] = {
    {"retrieve.expanded_query", getspec_bool, (char *) &isAugmented}, // is the query expanded and contain the lambda values 
    TRACE_PARAM ("retrieve.coll_sim.trace")
};
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static char *prefix;
static long num_wanted;
static char *textloc_file;      /* Used to tell how many docs in collection */
static SPEC_PARAM_PREFIX pspec_args[] = {
    {&prefix, "num_wanted",       getspec_long, (char *) &num_wanted},
    {&prefix, "doc.textloc_file", getspec_dbfile, (char *) &textloc_file},
};
static int num_pspec_args = sizeof (pspec_args) / sizeof (pspec_args[0]);


typedef struct {
    int valid_info;
    SM_INDEX_DOCDESC doc_desc;
    int *ctype_inst;
    long num_wanted;
    TOP_RESULT *top_results;
} STATIC_INFO;

static long max_did_in_coll = 0;
static float *full_results;
static STATIC_INFO *info;
static int max_inst = 0;

static int comp_top_sim();

int
init_sim_vec_inv_lm_nsim (spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    char param_buf[PATH_LEN];
    int new_inst, i;
    long max_did;
    REL_HEADER *rh;
    STATIC_INFO *ip;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);
    prefix = param_prefix;
    if (UNDEF == lookup_spec_prefix (spec, &pspec_args[0], num_pspec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_sim_vec_inv_lm_nsim");

    NEW_INST( new_inst );
    if (UNDEF == new_inst)
        return UNDEF;
    ip = &info[new_inst];
    ip->valid_info = 1;

    if (UNDEF == lookup_spec_docdesc (spec, &(ip->doc_desc)))
        return (UNDEF);

    /* Reserve space for inst id of each of the ctype retrieval procs */
    if (NULL == (ip->ctype_inst = (int *)
                 malloc ((unsigned) ip->doc_desc.num_ctypes * sizeof (int))))
        return (UNDEF);

    /* Call all initialization procedures */
    for (i = 0; i < ip->doc_desc.num_ctypes; i++) {
        (void) sprintf (param_buf, "%sctype.%d.", (prefix ? prefix : ""), i);
        if (UNDEF == (ip->ctype_inst[i] = ip->doc_desc.ctypes[i].inv_sim->
                      init_proc (spec, param_buf)))
            return (UNDEF);
    }

    /* Reserve space for top ranked documents */
    ip->num_wanted = num_wanted;
    if (NULL == (ip->top_results = Malloc (ip->num_wanted+1, TOP_RESULT)))
	return (UNDEF);

    /* Reserve space for all partial sims */
    if (NULL != (rh = get_rel_header (textloc_file)) && rh->max_primary_value)
        max_did = rh->max_primary_value;
    else
        max_did = MAX_DID_DEFAULT;

    if (max_did > max_did_in_coll) {
		if (max_did_in_coll) Free(full_results);
		max_did_in_coll = max_did;
		if (NULL == (full_results = Malloc (max_did_in_coll + 1, float))) {
	    	set_error (errno, "", "sim_vec_inv_lm_nsim");
		    return (UNDEF);
		}
    }

    PRINT_TRACE (2, print_string, "Trace: leaving init_sim_vec_inv_lm_nsim");
    return (new_inst);
}

int
sim_vec_inv_lm_nsim (in_retrieval, results, inst)
RETRIEVAL *in_retrieval;
RESULT *results;
int inst;
{
    QUERY_VECTOR *query_vec = in_retrieval->query;
    VEC *qvec = (VEC *) (query_vec->vector);
    long i;
	long ctype_len_buff[2] ;
    VEC ctype_query;
    CON_WT *con_wtp = NULL, *lambda_conwt_p = NULL;
    STATIC_INFO *ip = NULL;

    PRINT_TRACE (2, print_string, "Trace: entering sim_vec_inv_lm_nsim");
    PRINT_TRACE (6, print_vector, (VEC *) query_vec->vector);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "sim_vec_inv_lm_nsim");
        return (UNDEF);
    }
    ip  = &info[inst];

    results->qid = query_vec->qid;
    results->top_results = ip->top_results;
    results->num_top_results = ip->num_wanted;
    results->full_results = full_results;
    results->num_full_results = max_did_in_coll + 1;

    /* Ensure all values in full_results and top_results are -inf initially */
	memset(full_results, -1.0e8, sizeof (float) * (int) (max_did_in_coll+1));
	for (i = 0; i < (int)(ip->num_wanted+1); i++) {
		ip->top_results[i].did = 0;
		ip->top_results[i].sim = -1.0e8;
	}

    if (results->num_top_results > 0)
        results->min_top_sim = -1.0e8;  /* Allow negative numbers as sim values */
    else {
        /* Make it impossible for anything to enter top_docs */
        results->min_top_sim = 1.0e8;
    }

    /* Handle docs that have been previously seen by setting sim */
    /* to large negative number */
    for (i = 0; i < in_retrieval->tr->num_tr; i++)
        full_results[in_retrieval->tr->tr[i].did] = -1.0e8;

    /* Perform retrieval, sequentially going through query by ctype */
    ctype_query.id_num = qvec->id_num;
    ctype_query.num_ctype = 2;  // send the augmented lambda values accounting for the 2nd ctype
	ctype_query.ctype_len = ctype_len_buff ;

	// Initialize pointers to the ctype specific subvector and its corresponding
	// lambda values
    con_wtp = qvec->con_wtp;
	lambda_conwt_p = !isAugmented? NULL : con_wtp + half(qvec->num_conwt) ;
	
    for (i = 0; i < ip->doc_desc.num_ctypes; i++) {
        if (qvec->num_ctype <= i)
            break;
        if (qvec->ctype_len[i] <= 0)
            continue;

		// Form the subvectors by adjusting the lengths and the
		// weight pointers
        ctype_query.ctype_len[0] = qvec->ctype_len[i];
        ctype_query.ctype_len[1] = lambda_conwt_p == NULL ? 0 : qvec->ctype_len[i];
        ctype_query.num_conwt = ctype_query.ctype_len[0] + ctype_query.ctype_len[1] ;
		if ( lambda_conwt_p == NULL ) {
			ctype_query.con_wtp = con_wtp ;	
		}
		else {
	        ctype_query.con_wtp = (CON_WT*) malloc (sizeof(CON_WT) * ctype_query.num_conwt) ;
			if ( ctype_query.con_wtp == NULL )
				return UNDEF ;
			memcpy(ctype_query.con_wtp, con_wtp, ctype_query.ctype_len[0] * sizeof(CON_WT)) ;
			memcpy(ctype_query.con_wtp + ctype_query.ctype_len[0], lambda_conwt_p, ctype_query.ctype_len[1] * sizeof(CON_WT)) ;
		}

        con_wtp += qvec->ctype_len[i];
		if ( lambda_conwt_p != NULL ) {
			lambda_conwt_p += qvec->ctype_len[i] ;
		}

       	if (UNDEF == ip->doc_desc.ctypes[i].inv_sim->
            	proc (&ctype_query, results, ip->ctype_inst[i]))
	            return (UNDEF);

		// Free the allocated chunks
		if (lambda_conwt_p != NULL)
			FREE(ctype_query.con_wtp) ;
    }

    qsort ((char *) results->top_results,
           (int) results->num_top_results,
           sizeof (TOP_RESULT),
           comp_top_sim);

    /* Re-establish full_results value (ctype proc can realloc if needed) */
    full_results = results->full_results;
    max_did_in_coll = results->num_full_results - 1;

    PRINT_TRACE (8, print_full_results, results);
    PRINT_TRACE (4, print_top_results, results);
    PRINT_TRACE (2, print_string, "Trace: leaving sim_vec_inv_lm_nsim");
    return (1);
}


int
close_sim_vec_inv_lm_nsim (inst)
int inst;
{
    long i;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_sim_vec_inv_lm_nsim");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "sim_vec_inv_lm_nsim");
        return (UNDEF);
    }
    ip  = &info[inst];

    for (i = 0; i < ip->doc_desc.num_ctypes; i++) {
	if (UNDEF == (ip->doc_desc.ctypes[i].inv_sim->close_proc 
		      (ip->ctype_inst[i])))
	    return (UNDEF);
    }
    (void) free ((char *) ip->top_results);

    if (full_results)
	(void) free ((char *) full_results);

	FREE(ip->ctype_inst) ;

    ip->valid_info = 0;
    PRINT_TRACE (2, print_string, "Trace: leaving close_sim_vec_inv_lm_nsim");
    return (0);
}


static int
comp_top_sim (ptr1, ptr2)
TOP_RESULT *ptr1;
TOP_RESULT *ptr2;
{
    if (ptr1->sim > ptr2->sim ||
        (ptr1->sim == ptr2->sim &&
         ptr1->did > ptr2->did))
        return (-1);
    return (1);
}
