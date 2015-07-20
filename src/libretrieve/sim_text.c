#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libretrieve/sim_text.c,v 11.2 1993/02/03 16:54:44 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Take retrieval results from outside of SMART, pretend they came from SMART
 *1 retrieve.coll_sim.text
 *2 sim_text (in_retrieval, results, inst)
 *3   RETRIEVAL *in_retrieval;
 *3   RESULT *results;
 *3   int inst;
 *4 init_sim_text (spec, unused)
 *5   "retrieve.num_wanted"
 *5   "retrieve.doc.textloc_file"
 *5   "retrieve.sim_text_file"
 *5   "retrieve.rank_tr"
 *5   "retrieve.coll_sim.trace"
 *4 close_sim_text (inst)

 *7 sim_text_file is assumed to be lines of the form
 *7         qid did sim
 *7 sorted by qid.  These lines give the similarity sim of document did to
 *7 query qid and are assumed to give all non-zero similarities.
 *7 Sim_text assumes that there is a full indexed SMART version of the 
 *7 collection available, and the query id given by in_retrieval is
 *7 checked against the current query id in the text file.  All the did sims
 *7 for qid are returned in results->full_results.  The top sims seen are
 *7 kept track of by rank_tr and returned in results->top_results.

 *9 Should write a version that is completely independent of having a
 *9 SMART collection (will have to write a get_query routine to enable this).
 *9 Note that sim_text may have to be called by
 *9 retrieve.retrieve_all instead of retrieve.retrieve if sim_text_file
 *9 contains similarities for all queries in the query collection
 *9 (retrieve.retrieve skips those queries which do not have any relevant
 *9 documents, which may lead to a query id mismatch)
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


/* Get sim from text file giving tuples of form
       qid did sim
   File is sorted by qid
*/

/* Basic algorithm: malloc enough space to keep track of all similarities */
/*  of documents with direct access by did. */
/*  Top num_wanted similarites kept track of throughout. */

static long num_wanted;
static char *textloc_file;       /* Used to tell how many docs in collection */
static char *sim_text_file;      
static PROC_INST rank_tr;

static SPEC_PARAM spec_args[] = {
    {"retrieve.num_wanted",       getspec_long, (char *) &num_wanted},
    {"retrieve.doc.textloc_file", getspec_dbfile, (char *) &textloc_file},
    {"retrieve.sim_text_file",    getspec_dbfile, (char *) &sim_text_file},
    {"retrieve.rank_tr",          getspec_func, (char *) &rank_tr.ptab},
    TRACE_PARAM ("retrieve.coll_sim.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static float *full_results;
static TOP_RESULT *top_results;
static long max_did_in_coll;

static FILE *sim_fd;

static long last_qid, last_did;
static float last_sim;

int
init_sim_text (spec, unused)
SPEC *spec;
char *unused;
{
    REL_HEADER *rh;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_sim_text");

    /* Call all initialization procedures */
    if (UNDEF == (rank_tr.inst = rank_tr.ptab->init_proc (spec, NULL))) {
        return (UNDEF);
    }

    /* Reserve space for top ranked documents */
    if (NULL == (top_results = (TOP_RESULT *) malloc ((unsigned) (num_wanted+1)
                                                      * sizeof (TOP_RESULT))))
        return (UNDEF);

    /* Reserve space for all partial sims */
    if (NULL != (rh = get_rel_header (textloc_file)) && rh->max_primary_value)
        max_did_in_coll = rh->max_primary_value;
    else
        max_did_in_coll = MAX_DID_DEFAULT;

    if (NULL == (full_results = (float *) 
                 malloc ((unsigned) (max_did_in_coll + 1) * sizeof (float)))) {
        set_error (errno, "", "sim_text");
        return (UNDEF);
    }

    if (0 == strcmp (sim_text_file, "-"))
        sim_fd = stdin;
    else if (NULL == (sim_fd = fopen (sim_text_file, "r"))) {
        set_error (errno, sim_text_file, "sim_text");
        return (UNDEF);
    }

    if (3 != fscanf (sim_fd, "%ld %ld %f", &last_qid, &last_did, &last_sim)) {
        set_error (SM_INCON_ERR, "improper input", "sim_text");
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving init_sim_text");
    return (0);
}

int
sim_text (in_retrieval, results, inst)
RETRIEVAL *in_retrieval;
RESULT *results;
int inst;
{
    QUERY_VECTOR *query_vec = in_retrieval->query;
    long i;
    TOP_RESULT new_top;

    PRINT_TRACE (2, print_string, "Trace: entering sim_text");

    results->qid = query_vec->qid;
    results->top_results = top_results;
    results->num_top_results = num_wanted;
    results->full_results = full_results;
    results->num_full_results = max_did_in_coll + 1;

    /* Ensure all values in full_results and top_results are 0 initially */
    bzero ((char *) full_results, 
           sizeof (float) * (int) (max_did_in_coll+1));
    bzero ((char *) top_results, 
           sizeof (TOP_RESULT) * (int) (num_wanted + 1));

    /* Handle docs that have been previously seen by setting sim */
    /* to large negative number */
    for (i = 0; i < in_retrieval->tr->num_tr; i++)
        full_results[in_retrieval->tr->tr[i].did] = -1.0e8;

    /* Perform retrieval, sequentially going through query by ctype */
    if (query_vec->qid != last_qid) {
        set_error (SM_INCON_ERR, "Query id mismatch", "sim_text");
        return (UNDEF);
    }
    while (query_vec->qid == last_qid) {
        full_results[last_did] = last_sim;
        new_top.sim = last_sim;
        new_top.did = last_did;
        if (UNDEF == rank_tr.ptab->proc (&new_top, results, rank_tr.inst))
            return (UNDEF);
        if (3 != fscanf (sim_fd, "%ld %ld %f",
                         &last_qid, &last_did, &last_sim))
            last_qid = UNDEF;
    }

    PRINT_TRACE (8, print_full_results, results);
    PRINT_TRACE (4, print_top_results, results);
    PRINT_TRACE (2, print_string, "Trace: leaving sim_text");
    return (1);
}


int
close_sim_text (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_sim_text");

    if (UNDEF == rank_tr.ptab->close_proc(rank_tr.inst))
        return (UNDEF);
    if (sim_fd != stdin)
        (void) fclose (sim_fd);

    (void) free ((char *) top_results);
    (void) free ((char *) full_results);
    
    PRINT_TRACE (2, print_string, "Trace: leaving close_sim_text");
    return (0);
}

