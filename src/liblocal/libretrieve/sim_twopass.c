#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libretrieve/trec_twopass.c,v 11.2 1993/02/03 16:52:56 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Match whole query versus doc collection using inverted search
 *1 retrieve.coll_sim.twopass
 *1 retrieve.coll_sim.inverted
 *2 sim_twopass (in_retrieval, results, inst)
 *3   RETRIEVAL *in_retrieval;
 *3   RESULT *results;
 *3   int inst;
 *4 init_sim_twopass (spec, unused)
 *5   "retrieve.num_wanted"
 *5   "retrieve.textloc_file"
 *5   "index.ctype.*.inv_sim"
 *5   "retrieve.coll_sim.trace"
 *4 close_sim_twopass (inst)

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

static char *textloc_file;      /* Used to tell how many docs in collection */

static SPEC_PARAM spec_args[] = {
    {"retrieve.doc.textloc_file", getspec_dbfile, (char *) &textloc_file},
    TRACE_PARAM ("retrieve.coll_sim.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static char *prefix;
static long num_wanted_param;
static SPEC_PARAM_PREFIX pspec_args[] = {
    {&prefix,  "num_wanted",       getspec_long, (char *) &num_wanted_param},
    };
static int num_pspec_args = sizeof (pspec_args) / sizeof (pspec_args[0]);

typedef struct {
    int valid_info;     /* bookkeeping */

    SM_INDEX_DOCDESC doc_desc;
    float *full_results;
    TOP_RESULT *top_results;
    long max_did_in_coll;
    
    int *ctype_inst;

    int num_wanted;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static int comp_top_sim();

int
init_sim_twopass (spec, passed_prefix)
SPEC *spec;
char *passed_prefix;
{
    STATIC_INFO *ip;
    int new_inst;
    REL_HEADER *rh;
    long i;
    char param_prefix[PATH_LEN];

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    prefix = passed_prefix;
    if (UNDEF == lookup_spec_prefix (spec, &pspec_args[0], num_pspec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_sim_twopass");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    ip = &info[new_inst];

    ip->num_wanted = num_wanted_param;

    if (UNDEF == lookup_spec_docdesc (spec, &ip->doc_desc))
        return (UNDEF);

    /* Reserve space for inst id of each of the ctype retrieval procs */
    if (NULL == (ip->ctype_inst = (int *)
                 malloc ((unsigned) ip->doc_desc.num_ctypes * sizeof (int))))
        return (UNDEF);

    /* Call all initialization procedures */
    for (i = 0; i < ip->doc_desc.num_ctypes; i++) {
        (void) sprintf (param_prefix, "ctype.%ld.", i);
        if (UNDEF == (ip->ctype_inst[i] = ip->doc_desc.ctypes[i].inv_sim->
                      init_proc (spec, param_prefix)))
            return (UNDEF);
    }

    /* Reserve space for top ranked documents */
    if (NULL == (ip->top_results = (TOP_RESULT *)
                 malloc ((unsigned) (ip->num_wanted+1) * sizeof (TOP_RESULT))))
        return (UNDEF);

    /* Reserve space for all partial sims */
    if (NULL != (rh = get_rel_header (textloc_file)) && rh->max_primary_value)
        ip->max_did_in_coll = rh->max_primary_value;
    else
        ip->max_did_in_coll = MAX_DID_DEFAULT;

    if (NULL == (ip->full_results = (float *) 
                 malloc ((unsigned) (ip->max_did_in_coll + 1) *
                         sizeof (float)))) {
        set_error (errno, "", "sim_twopass");
        return (UNDEF);
    }

    ip->valid_info++;
    PRINT_TRACE (2, print_string, "Trace: leaving init_sim_twopass");
    return (new_inst);
}

int
sim_twopass (in_retrieval, results, inst)
RETRIEVAL *in_retrieval;
RESULT *results;
int inst;
{
    STATIC_INFO *ip;
    QUERY_VECTOR *query_vec = in_retrieval->query;
    VEC *qvec = (VEC *) (query_vec->vector);
    long i;
    VEC ctype_query;
    CON_WT *con_wtp;

    PRINT_TRACE (2, print_string, "Trace: entering sim_twopass");
    PRINT_TRACE (6, print_vector, (VEC *) query_vec->vector);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "sim_ctype_inv");
        return (UNDEF);
    }
    ip  = &info[inst];

    results->qid = query_vec->qid;
    results->top_results = ip->top_results;
    results->num_top_results = ip->num_wanted;
    results->full_results = ip->full_results;
    results->num_full_results = ip->max_did_in_coll + 1;

    /* Ensure all values in full_results and top_results are 0 initially */
    bzero ((char *) ip->full_results, 
           sizeof (float) * (int) (ip->max_did_in_coll+1));
    bzero ((char *) ip->top_results, 
           sizeof (TOP_RESULT) * (int) (ip->num_wanted + 1));
    if (results->num_top_results > 0)
        results->min_top_sim = 0.0;
    else {
        /* Make it impossible for anything to enter top_docs */
        results->min_top_sim = 1.0e8;
    }

    /* Handle docs that have been previously seen by setting sim */
    /* to large negative number */
    for (i = 0; i < in_retrieval->tr->num_tr; i++)
        ip->full_results[in_retrieval->tr->tr[i].did] = -1.0e8;

    /* Perform retrieval, sequentially going through query by ctype */
    ctype_query.id_num = qvec->id_num;
    ctype_query.num_ctype = 1;
    con_wtp = qvec->con_wtp;
    for (i = 0; i < ip->doc_desc.num_ctypes; i++) {
        if (qvec->num_ctype <= i)
            break;
        if (qvec->ctype_len[i] <= 0)
            continue;

        ctype_query.ctype_len = &qvec->ctype_len[i];
        ctype_query.num_conwt = qvec->ctype_len[i];
        ctype_query.con_wtp = con_wtp;
        con_wtp += qvec->ctype_len[i];
        if (UNDEF == ip->doc_desc.ctypes[i].inv_sim->
            proc (&ctype_query, results, ip->ctype_inst[i]))
            return (UNDEF);
    }

    qsort ((char *) results->top_results,
           (int) results->num_top_results,
           sizeof (TOP_RESULT),
           comp_top_sim);

    /* Re-establish full_results value (ctype proc can realloc if needed) */
    ip->full_results = results->full_results;
    ip->max_did_in_coll = results->num_full_results - 1;

    PRINT_TRACE (8, print_full_results, results);
    PRINT_TRACE (4, print_top_results, results);
    PRINT_TRACE (2, print_string, "Trace: leaving sim_twopass");
    return (1);
}


int
close_sim_twopass (inst)
int inst;
{
    STATIC_INFO *ip;
    long i;
    PRINT_TRACE (2, print_string, "Trace: entering close_sim_twopass");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "ctype_inv");
        return (UNDEF);
    }
    ip  = &info[inst];
    ip->valid_info--;

    for (i = 0; i < ip->doc_desc.num_ctypes; i++) {
        if (UNDEF == (ip->doc_desc.ctypes[i].inv_sim->close_proc (ip->ctype_inst[i])))
            return (UNDEF);
    }
    (void) free ((char *) ip->top_results);
    (void) free ((char *) ip->full_results);
    (void) free ((char *) ip->ctype_inst);
    
    PRINT_TRACE (2, print_string, "Trace: leaving close_sim_twopass");
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
