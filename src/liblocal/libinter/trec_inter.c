#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libinter/trec_inter.c,v 11.2 1993/02/03 16:51:22 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "textloc.h"
#include "io.h"
#include "smart_error.h"
#include "spec.h"
#include "proc.h"
#include "sm_display.h"
#include "tr_vec.h"
#include "rr_vec.h"
#include "context.h"
#include "retrieve.h"
#include "inter.h"
#include "inst.h"
#include "trace.h"
#include "vector.h"

#define NUM_WANTED 10

static char *qrels_file;
static long qrels_mode;
static SPEC_PARAM spec_args[] = {
    {"inter.trec_inter.qrels_file", getspec_dbfile, (char *) &qrels_file},
    {"inter.trec_inter.qrels_file.rmode", getspec_filemode, (char *) &qrels_mode},
    TRACE_PARAM ("inter.trec_inter.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    int set_query_inst;
    int run_vecs_inst;
    int qrels_fd;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static int compare_tr_rank ();
static int compare_tr_did ();

int init_set_query(), init_run_vec();
int set_query(), run_qvec();
int close_set_query(), close_run_vec();
int run_trec_feedback();

int
init_trec_inter (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_trec_inter");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];

    if (UNDEF == (ip->set_query_inst = init_set_query(spec, (char *) NULL)) ||
	UNDEF == (ip->run_vecs_inst = init_run_vec(spec, (char *) NULL)))
	return UNDEF;
    if (! VALID_FILE (qrels_file) ||
        UNDEF == (ip->qrels_fd = open_rr_vec (qrels_file, qrels_mode)))
        return (UNDEF);

    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_trec_inter");
    return (new_inst);
}

int
trec_inter(is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    STATIC_INFO *ip;
    RR_VEC qrels_vec;
    TR_VEC *tr_vec;
    long i, qrels_index, tr_index, fdbk_iter = 0, num_seen = 0;

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.run_retrieve");
        return (UNDEF);
    }
    PRINT_TRACE (2, print_string, "Trace: entering trec_inter");
    ip  = &info[inst];

    if (is->num_command_line < 3 ||
	(0 == (num_seen = atol (is->command_line[2])))) {
	printf("ITERATIONS NOT SPECIFIED\n");
	return UNDEF;
    }

    fdbk_iter = num_seen/NUM_WANTED;

    if (UNDEF == set_query(is, (char *) NULL, ip->set_query_inst) ||
	UNDEF == run_qvec(is, (char *) NULL, ip->run_vecs_inst))
	return UNDEF;
    /* Restore tr_vec to be sorted in increasing did order */
    if (is->retrieval.tr->num_tr > 0)
        qsort ((char *) is->retrieval.tr->tr,
               (int) is->retrieval.tr->num_tr,
               sizeof (TR_TUP), compare_tr_did);

    tr_vec = is->retrieval.tr;
    qrels_vec.qid = tr_vec->qid;
    if (1 != seek_rr_vec (ip->qrels_fd, &qrels_vec)) {
	printf("QUERY HAS NO RELEVANT DOCUMENTS\n");
	return UNDEF;
    }
    if (1 != read_rr_vec (ip->qrels_fd, &qrels_vec))
	return (UNDEF);
    /* add relevance judgments */
    for (tr_index = 0, qrels_index = 0;
	 tr_index < tr_vec->num_tr && qrels_index < qrels_vec.num_rr;
	 ) {
	if (tr_vec->tr[tr_index].did < qrels_vec.rr[qrels_index].did) {
	    tr_vec->tr[tr_index].rel = 0;
	    tr_index++;
	}
	else if (tr_vec->tr[tr_index].did > qrels_vec.rr[qrels_index].did)
	    qrels_index++;
	else {
	    tr_vec->tr[tr_index].rel = 1;
	    tr_index++;
	    qrels_index++;
	}
    }
    while (tr_index < tr_vec->num_tr) {
	tr_vec->tr[tr_index].rel = 0;
	tr_index++;
    }

    for (i = 0; i < fdbk_iter; i++) {
	if (UNDEF == run_trec_feedback(is, (char *) NULL, ip->run_vecs_inst))
	    return UNDEF;

	/* Restore tr_vec to be sorted in increasing did order */
	if (is->retrieval.tr->num_tr > 0)
	    qsort ((char *) is->retrieval.tr->tr,
		   (int) is->retrieval.tr->num_tr,
		   sizeof (TR_TUP), compare_tr_did);

        /* add relevance judgments */
        for (tr_index = 0, qrels_index = 0;
             tr_index < tr_vec->num_tr && qrels_index < qrels_vec.num_rr;
             ) {
            if (tr_vec->tr[tr_index].did < qrels_vec.rr[qrels_index].did) {
                tr_vec->tr[tr_index].rel = 0;
                tr_index++;
            }
            else if (tr_vec->tr[tr_index].did > qrels_vec.rr[qrels_index].did)
                qrels_index++;
            else {
                tr_vec->tr[tr_index].rel = 1;
                tr_index++;
                qrels_index++;
            }
        }
        while (tr_index < tr_vec->num_tr) {
            tr_vec->tr[tr_index].rel = 0;
            tr_index++;
        }
    }

    /* Sort tr_vec by iteration and rank */
    if (is->retrieval.tr->num_tr > 0)
	qsort ((char *) is->retrieval.tr->tr,
	       (int) is->retrieval.tr->num_tr,
	       sizeof (TR_TUP), compare_tr_rank);

    is->retrieval.tr->num_tr = num_seen;

    /* Restore tr_vec to be sorted in increasing did order */
    if (is->retrieval.tr->num_tr > 0)
	qsort ((char *) is->retrieval.tr->tr,
	       (int) is->retrieval.tr->num_tr,
	       sizeof (TR_TUP), compare_tr_did);    

    PRINT_TRACE (4, print_tr_vec , is->retrieval.tr);
    PRINT_TRACE (2, print_string, "Trace: leaving trec_inter");
    return (1);
}

int
close_trec_inter(inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_trec_inter");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.run_vec");
        return (UNDEF);
    }
    ip  = &info[inst];
    
    /* Check to see if still have valid instantiations using this data */
    if (--ip->valid_info > 0)
        return (0);

    if (UNDEF == close_set_query(ip->set_query_inst) ||
	UNDEF == close_run_vec(ip->run_vecs_inst))
	return UNDEF;

    PRINT_TRACE (2, print_string, "Trace: leaving close_trec_inter");

    return (1);
}

/******************************************************************
 * A comparison routine that operates on iteration/rank
 ******************************************************************/
static int
compare_tr_rank (tr1, tr2)
register TR_TUP *tr1;
register TR_TUP *tr2;
{
    if (tr1->iter != tr2->iter) {
        if (tr1->iter > tr2->iter)
            return (1);
        return (-1);
    }
    if (tr1->rank < tr2->rank) {
        return (-1);
    }
    if (tr1->rank > tr2->rank) {
        return (1);
    }
    return (0);
}

/******************************************************************
 * A comparison routine that operates on simple docids.
 ******************************************************************/
static int
compare_tr_did( t1, t2 )
TR_TUP *t1, *t2;
{
    if (t1->did > t2->did)
        return 1;
    if (t1->did < t2->did)
        return -1;

    return 0;
}
