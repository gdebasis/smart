#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libretrieve/ret_tr_rr.c,v 11.2 1993/02/03 16:54:36 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Compute and store query retrieval results in both tr file and rr file
 *1 retrieve.output.ret_tr_rr
 *2 ret_tr_rr (results, tr_vec, inst)
 *3   RESULT *results;
 *3   TR_VEC *tr_vec;
 *3   int inst;
 *4 init_ret_tr_rr (spec, unused)
 *5   "retrieve.tr_file"
 *5   "retrieve.tr_file.rwmode"
 *5   "retrieve.rr_file"
 *5   "retrieve.rr_file.rwmode"
 *5   "retrieve.qrels_file"
 *5   "retrieve.qrels_file.rmode"
 *5   "retrieve.doc.textloc_file"
 *5   "retrieve.output.trace"
 *4 close_ret_tr_rr (inst)
 *7 Add results of a single query to tr_vec. tr_vec upon input has info 
 *7 from any previous iteration of this query.  If tr_file is a valid file
 *7 name, then also write out the top results to tr_file. The ranks of
 *7 all relevant docs for this query are also computed and stored in
 *7 rr_file
 *9 Note: breaking similarity ties when ranking rel documents is done
 *9 by ranking the highest doc_id first.  If a different method is used for
 *9 breaking ties when originally finding the top docs (see procedure 
 *9 retrieve.rank_tr), then the tr and rr files will be incompatible.
***********************************************************************/


#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "io.h"
#include "rel_header.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "context.h"
#include "retrieve.h"
#include "rr_vec.h"
#include "tr_vec.h"

static char *tr_file;
static long tr_file_mode;
static char *rr_file;
static long rr_file_mode;
static char *qrels_file;
static long qrels_file_mode;
static char *textloc_file;          /* Gives num_docs in collection */

static SPEC_PARAM spec_args[] = {
    {"retrieve.tr_file",          getspec_dbfile, (char *) &tr_file},
    {"retrieve.tr_file.rwmode",   getspec_filemode, (char *) &tr_file_mode},
    {"retrieve.rr_file",          getspec_dbfile, (char *) &rr_file},
    {"retrieve.rr_file.rwmode",   getspec_filemode, (char *) &rr_file_mode},
    {"retrieve.qrels_file",       getspec_dbfile, (char *) &qrels_file},
    {"retrieve.qrels_file.rmode", getspec_filemode, (char *) &qrels_file_mode},
    {"retrieve.doc.textloc_file",    getspec_dbfile, (char *) &textloc_file},
    TRACE_PARAM ("retrieve.output.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static int tr_fd;
static int rr_fd;
static int qrels_fd;

static RR_VEC qrels;

static long num_docs;

static int compare_sim_did(), compare_did();

int
init_ret_tr_rr (spec, unused)
SPEC *spec;
char *unused;
{
    REL_HEADER *rh;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_ret_tr_rr");

    tr_fd = rr_fd = qrels_fd = UNDEF;

    /* Open tr file */
    if (VALID_FILE (tr_file)) {
        if (UNDEF == (tr_fd = open_tr_vec (tr_file, tr_file_mode))) {
            clr_err();
            if (UNDEF == (tr_fd = open_tr_vec (tr_file, tr_file_mode|SCREATE)))
                return (UNDEF);
        }
    }

    /* Open rr file */
    if (VALID_FILE (rr_file)) {
        if (UNDEF == (rr_fd = open_rr_vec (rr_file, rr_file_mode))) {
            clr_err();
            if (UNDEF == (rr_fd = open_rr_vec (rr_file, rr_file_mode|SCREATE)))
                return (UNDEF);
        }
    }

    /* Open qrels file */
    if (VALID_FILE (qrels_file) &&
        UNDEF == (qrels_fd = open_rr_vec (qrels_file, qrels_file_mode)))
        return (UNDEF);

    /* Find the number of actual docs in collection */
    if (NULL != (rh = get_rel_header (textloc_file)) && rh->num_entries != 0)
        num_docs = rh->num_entries;
    else {
        set_error (SM_INCON_ERR, "Can't get actual num_docs", "ret_tr_rr");
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving init_ret_tr_rr");
    return (0);
}

/* Add results in proper format to tr_vec */
/* tr_vec upon input has info from any previous iteration of this query */
/* Add top results to tr_file if designated */
/* Add relevant results to rr_file if designated */
int
ret_tr_rr (results, tr_vec, inst)
RESULT *results;
TR_VEC *tr_vec;
int inst;
{
    RR_TUP *qr_ptr, *end_qr_ptr;
    RR_TUP *rr_ptr, *end_zero_rr;
    RR_TUP *end_rr_ptr = NULL;
    long num_zero_docs_seen;
    long did;
    float *full_results = results->full_results;
    long num_full_results = results->num_full_results;
    RR_VEC rr_vec;
    long i;
    int status;

    PRINT_TRACE (2, print_string, "Trace: entering ret_tr_rr");

    if (tr_vec == (TR_VEC *) NULL)
        return (UNDEF);

    tr_vec->qid = results->qid;
    rr_vec.qid = results->qid;
    qrels.qid = results->qid;

    if (UNDEF == (status = seek_rr_vec (qrels_fd, &qrels)))
        return (UNDEF);
    if (status == 0) {
        /* No relevant docs */
        rr_vec.num_rr = 0;
        qrels.num_rr = 0;
    }
    else {
        if (UNDEF == read_rr_vec (qrels_fd, &qrels))
            return (UNDEF);
    }

    if (qrels.num_rr > 0) {
        if (NULL == (rr_vec.rr = (RR_TUP *)
                     malloc ((unsigned) qrels.num_rr * sizeof (RR_TUP))))
            return (UNDEF);
    
        /* Remove relevant seen docs from qrels, keeping qrels sorted */
        /* by did, without holes by copying unseen entries into new_ptr */
        /* Note that seen_docs and qrels both sorted by did within qid */
        qr_ptr = qrels.rr;
        end_qr_ptr = &qrels.rr[qrels.num_rr];
        end_rr_ptr = rr_vec.rr;
        for (i = 0; i < tr_vec->num_tr; i++) {
            while (qr_ptr < end_qr_ptr && tr_vec->tr[i].did > qr_ptr->did)
                (end_rr_ptr++)->did = (qr_ptr++)->did;
            if (qr_ptr < end_qr_ptr && tr_vec->tr[i].did == qr_ptr->did) {
                /* skip over qr_ptr since has been seen previously */
                qr_ptr++;
            }
        }
        while (qr_ptr < end_qr_ptr) {
            (end_rr_ptr++)->did = (qr_ptr++)->did;
        }
        rr_vec.num_rr = end_rr_ptr - rr_vec.rr;
    }
    else {
        rr_vec.num_rr = 0;
    }

    /* Add top_results to tr_vec.  Possibly add relevance info and write out */
    if (UNDEF == res_tr (results, tr_vec, 0))
        return (UNDEF);
    if (qrels_fd != UNDEF && rr_vec.num_rr > 0) {
        /* Get relevance judgement for all docs */
        rr_ptr = rr_vec.rr;
        for (i = 0; i < tr_vec->num_tr; i++) {
            tr_vec->tr[i].rel = 0;
            while (rr_ptr < end_rr_ptr && tr_vec->tr[i].did >rr_ptr->did)
                rr_ptr++;
            if (rr_ptr < end_rr_ptr && tr_vec->tr[i].did == rr_ptr->did){
                rr_ptr++;
                tr_vec->tr[i].rel = 1;
            }
        }
    }
    /* output tr_vec to tr_file if desired */
    if (UNDEF != tr_fd && tr_vec->num_tr > 0) {
        if (UNDEF == seek_tr_vec (tr_fd, tr_vec) ||
            UNDEF == write_tr_vec (tr_fd, tr_vec)) 
            return (UNDEF);
    }

    /* Construct rr relation if needed and desired */
    if (rr_fd != UNDEF && rr_vec.num_rr > 0) {
        /* Get similarity for each of rr_vec. */
        /* Initialize all ranks to 1 */
        for (rr_ptr = rr_vec.rr; rr_ptr < end_rr_ptr; rr_ptr++) {
            rr_ptr->sim = full_results[rr_ptr->did];
            rr_ptr->rank = 1;
        }

        /* Sort retrieved rel docs by increasing <sim, did> */
        qsort ((char *) rr_vec.rr,
               (int) rr_vec.num_rr,
               sizeof (RR_TUP),
               compare_sim_did);
        
        /* Determine where break between zero and non-zero qrels is */
        for (rr_ptr = rr_vec.rr;
             rr_ptr < end_rr_ptr && rr_ptr->sim == 0.0;
             rr_ptr++)
            ;
        end_zero_rr = rr_ptr;
        
        /* Define ranks for each of qrels by counting number */
        /* of entries in full_results that are greater than it. */
        /* Warning: a zero sim could be due to either a document not */
        /* matching or a document not existing.  Handled by assuming that  */
        /* first (num_full_results - num_docs) docs with sim == 0.0  */
        /* do not exist. */
        num_zero_docs_seen = 0;
        for (did = 0; did < num_full_results; did++) {
            if (0.0 == full_results[did]) {
                if (++num_zero_docs_seen > num_full_results - num_docs) {
                    for (rr_ptr = rr_vec.rr; 
                         rr_ptr < end_zero_rr && did > rr_ptr->did;
                         rr_ptr++)
                        rr_ptr->rank++;
                }
            }
            else {
                for (rr_ptr = rr_vec.rr;
                     rr_ptr < end_rr_ptr && 
                     (full_results[did] > rr_ptr->sim || 
                      (full_results[did] == rr_ptr->sim && 
                       did > rr_ptr->did));
                     rr_ptr++)
                    rr_ptr->rank++;
            }
        }
        
        /* Resort rel docs by increasing did */
        qsort ((char *) rr_vec.rr,
               (int) rr_vec.num_rr,
               sizeof (RR_TUP),
               compare_did);
        
        /* Write the new rr_relation out */
        if (rr_vec.num_rr > 0 &&
            (UNDEF == seek_rr_vec (rr_fd, &rr_vec) ||
             1 != write_rr_vec (rr_fd, &rr_vec)))
            return (UNDEF);

        (void) free ((char *) rr_vec.rr);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving ret_tr_rr");
    return (1);
}


int
close_ret_tr_rr (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_ret_tr_rr");

    if (UNDEF != tr_fd && UNDEF == close_tr_vec (tr_fd))
        return (UNDEF);
    if (UNDEF != rr_fd && UNDEF == close_rr_vec (rr_fd))
        return (UNDEF);
    if (UNDEF != qrels_fd && UNDEF == close_rr_vec (qrels_fd))
        return (UNDEF);

    tr_fd = rr_fd = qrels_fd = UNDEF;

    PRINT_TRACE (2, print_string, "Trace: leaving close_ret_tr_rr");
    return (0);
}


static int
compare_sim_did (rr1, rr2)
register RR_TUP *rr1;
register RR_TUP *rr2;
{
    if ( rr1->sim < rr2->sim || 
         (rr1->sim == rr2->sim && rr1->did < rr2->did)) {
        return (-1);
    }
    return (1);
}

static int
compare_did (rr1, rr2)
register RR_TUP *rr1;
register RR_TUP *rr2;
{
    return (rr1->did - rr2->did);
}
