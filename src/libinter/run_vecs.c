#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libinter/run_vecs.c,v 11.2 1993/02/03 16:51:22 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "textloc.h"
#include "io.h"
#include "smart_error.h"
#include "spec.h"
#include "proc.h"
#include "sm_display.h"
#include "docindex.h"
#include "tr_vec.h"
#include "context.h"
#include "retrieve.h"
#include "inter.h"
#include "inst.h"
#include "trace.h"
#include "vector.h"


static PROC_TAB *p_retrieve_ptab;
static PROC_TAB *p_feedback_ptab;
static PROC_TAB *get_query_ptab;
static PROC_TAB *preparse_query_ptab;
static PROC_TAB *index_query_ptab;

static SPEC_PARAM spec_args[] = {
    {"inter.coll_sim",       getspec_func,    (char *) &p_retrieve_ptab},
    {"inter.feedback",       getspec_func,    (char *) &p_feedback_ptab},
    {"inter.get_query",      getspec_func,    (char *) &get_query_ptab},
    {"inter.query.preparser", getspec_func,    (char *) &preparse_query_ptab},
    {"inter.query.index_pp", getspec_func,    (char *) &index_query_ptab},
    TRACE_PARAM ("inter.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    SPEC *saved_spec;

    VEC qvec;             /* Current query */

    PROC_INST p_retrieve;
    PROC_INST p_feedback;
    PROC_INST get_query;
    PROC_INST preparse_query;
    PROC_INST index_query;

    int next_vecid_1_inst;
    int util_inst;
    int title_inst;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static int compare_tr_did();
static int compare_tr_rel_did();

int init_next_vecid_1(), next_vecid_1(), close_next_vecid_1();

int
init_run_vec (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;
    long i;
    CONTEXT old_context;

    /* Check to see if this spec file is already initialized (only need
       one initialization per spec file) */
    for (i = 0; i <  max_inst; i++) {
        if (info[i].valid_info && spec == info[i].saved_spec ) {
            info[i].valid_info++;
            return (i);
        }
    }

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_run_vec");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];

    ip->saved_spec = spec;

    /* Initialize other procedures */
    /* Set context indicating that we are indexing a query.  Tells
       inferior procedures to use params appropriate for query instead of
       query */
    old_context = get_context();
    set_context (CTXT_INDEXING_QUERY);
    ip->p_retrieve.ptab = p_retrieve_ptab;
    ip->p_feedback.ptab = p_feedback_ptab;
    ip->get_query.ptab = get_query_ptab;
    ip->preparse_query.ptab = preparse_query_ptab;
    ip->index_query.ptab = index_query_ptab;
    if (UNDEF == (ip->p_retrieve.inst =
                  ip->p_retrieve.ptab->init_proc (spec, NULL)) ||
        UNDEF == (ip->p_feedback.inst =
                  ip->p_feedback.ptab->init_proc (spec, NULL)) ||
        UNDEF == (ip->get_query.inst =
                  ip->get_query.ptab->init_proc (spec, "query.")) ||
        UNDEF == (ip->preparse_query.inst =
                  ip->preparse_query.ptab->init_proc (spec, "query.")) ||
        UNDEF == (ip->index_query.inst =
                  ip->index_query.ptab->init_proc (spec, "query.")))
        return (UNDEF);
    set_context (old_context);

    if (UNDEF == (ip->util_inst = init_inter_util (spec, (char *) NULL)))
        return (UNDEF);
    if (UNDEF == (ip->title_inst = init_show_titles (spec, (char *) NULL)))
        return (UNDEF);
    if (UNDEF == (ip->next_vecid_1_inst = init_next_vecid_1 (spec, (char *) NULL)))
        return (UNDEF);

    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_run_vec");
    return (new_inst);
}


int
close_run_vec (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_run_vec");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.run_vec");
        return (UNDEF);
    }
    ip  = &info[inst];
    
    /* Check to see if still have valid instantiations using this data */
    if (--ip->valid_info > 0)
        return (0);
    
    if (UNDEF == ip->p_retrieve.ptab->close_proc (ip->p_retrieve.inst) ||
        UNDEF == ip->p_feedback.ptab->close_proc (ip->p_feedback.inst) ||
        UNDEF == ip->get_query.ptab->close_proc (ip->get_query.inst) ||
        UNDEF == ip->preparse_query.ptab->close_proc (ip->preparse_query.inst) ||
        UNDEF == ip->index_query.ptab->close_proc (ip->index_query.inst))
        return (UNDEF);
    
    if (UNDEF == close_inter_util (ip->util_inst))
        return (UNDEF);
    if (UNDEF == close_show_titles (ip->title_inst))
        return (UNDEF);
    if (UNDEF == close_next_vecid_1 (ip->next_vecid_1_inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_run_vec");

    return (1);
}

/* Run a retrieval with query is->retrieval.query, and seen_docs
   is->retrieval.tr, adding results to is->retrieval.tr */
static int
run_retrieve (is, inst)
INTER_STATE *is;
int inst;
{
    RESULT results;          /* Full results of current query */
    STATIC_INFO *ip;

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.run_retrieve");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Restore tr_vec to be sorted in increasing did order */
    if (is->retrieval.tr->num_tr > 0)
        qsort ((char *) is->retrieval.tr->tr,
               (int) is->retrieval.tr->num_tr,
               sizeof (TR_TUP), compare_tr_did);

    if (UNDEF == ip->p_retrieve.ptab->proc (&is->retrieval,
                                        &results,
                                        ip->p_retrieve.inst))
        return (UNDEF);
    if (UNDEF == res_tr (&results, is->retrieval.tr))
        return (UNDEF);

    if (UNDEF == inter_prepare_titles(is, (char *) NULL, ip->title_inst))
        return (UNDEF);

    is->current_doc = 0;

    if (is->verbose_level)
        return (inter_show_titles(is, (char *) NULL, ip->title_inst));

    return (1);
}

int
run_qvec (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    int status;

    PRINT_TRACE (2, print_string, "Trace: entering run_qvec");

    if (is->retrieval.query->vector == NULL) {
        if (UNDEF == add_buf_string ("No valid query.  Command ignored\n",
                                     &is->err_buf))
            return (UNDEF);
        return (0);
    }

    status = run_retrieve (is, inst);

    PRINT_TRACE (2, print_string, "Trace: leaving run_qvec");

    return (status);
}

int
run_dvec (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    VEC temp_vec;
    STATIC_INFO *ip;
    int status;

    PRINT_TRACE (2, print_string, "Trace: entering run_dvec");
    
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.run_dvec");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (is->num_command_line != 2 ||
        UNDEF == inter_get_vec (is->command_line[1],&temp_vec, ip->util_inst)){
        if (UNDEF == add_buf_string (
                              "Not a valid single document. Command ignored\n",
                               &is->err_buf))
            return (UNDEF);
        return (0);
    }

    /* MEMORY LEAK */
    /* Should save qvec lists in STATIC_INFO */
    if (UNDEF == copy_vec (&ip->qvec, &temp_vec))
        return (UNDEF);

    is->retrieval.tr->num_tr = 0;
    is->retrieval.query->vector = (char *) &ip->qvec;
    is->retrieval.query->qid = ip->qvec.id_num.id;

    status = run_retrieve (is, inst);

    PRINT_TRACE (2, print_string, "Trace: leaving run_dvec");

    return (status);
}


int
run_feedback(is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    QUERY_VECTOR fdbk_query;
    TR_VEC *tr_vec;
    long command, i;
    char temp_buf[PATH_LEN];
    long temp_did;
    int status;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering run_feedback");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.run_feedback");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (is->retrieval.query->vector == (char *) NULL) {
        if (UNDEF == add_buf_string ("No query to run feedback on.",
                                     &is->err_buf))
            return (UNDEF);
        return (0);
    }

    /* Docids on command line are relevant */
    tr_vec = is->retrieval.tr;
    for (command = 1; command < is->num_command_line; command++) {
        temp_did = atol (is->command_line[command]);
        for (i = 0;
             i < tr_vec->num_tr && temp_did != tr_vec->tr[i].did;
             i++)
            ;
        if (i >= tr_vec->num_tr) {
            (void) sprintf (temp_buf,
                            "Document %s cannot be used for feedback.",
			    is->command_line[command]);
            if (UNDEF == add_buf_string (temp_buf, &is->err_buf))
                return (UNDEF);
            return (0);
        }
        tr_vec->tr[i].rel = 1;
    }
    
    /* Restore tr_vec to be sorted in increasing did order */
    if (is->retrieval.tr->num_tr > 0)
        qsort ((char *) is->retrieval.tr->tr,
               (int) is->retrieval.tr->num_tr,
               sizeof (TR_TUP), compare_tr_did);

    /* Create feedback query */
    if (UNDEF == ip->p_feedback.ptab->proc (&is->retrieval,
                                        &fdbk_query,
                                        ip->p_feedback.inst))
        return (UNDEF);

    if (fdbk_query.vector != NULL)
        is->retrieval.query->vector = fdbk_query.vector;
    

    status = run_retrieve (is, inst);

    PRINT_TRACE (2, print_string, "Trace: leaving run_feedback");

    return (status);
}


int
run_trec_feedback(is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    QUERY_VECTOR fdbk_query;
    int status, i;
    STATIC_INFO *ip;
    long save = is->retrieval.tr->num_tr;

    PRINT_TRACE (2, print_string, "Trace: entering run_trec_feedback");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.run_feedback");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Remove don't cares */
    if (is->retrieval.tr->num_tr > 0)
    {
        qsort ((char *) is->retrieval.tr->tr,
               (int) is->retrieval.tr->num_tr,
               sizeof (TR_TUP), compare_tr_rel_did);
	for (i = 0; i < is->retrieval.tr->num_tr; i++)
	    if (is->retrieval.tr->tr[i].rel == UNDEF)
		break;
	is->retrieval.tr->num_tr = i;
    }

    /* Restore tr_vec to be sorted in increasing did order */
    if (is->retrieval.tr->num_tr > 0)
        qsort ((char *) is->retrieval.tr->tr,
               (int) is->retrieval.tr->num_tr,
               sizeof (TR_TUP), compare_tr_did);

    /* Create feedback query */
    if (UNDEF == ip->p_feedback.ptab->proc (&is->retrieval,
                                        &fdbk_query,
                                        ip->p_feedback.inst))
        return (UNDEF);
    if (fdbk_query.vector != NULL)
        is->retrieval.query->vector = fdbk_query.vector;

    /* add back the don't cares */
    is->retrieval.tr->num_tr = save;

    status = run_retrieve (is, inst);

    PRINT_TRACE (2, print_string, "Trace: leaving run_trec_feedback");

    return (status);
}


/******************************************************************
 *
 * Start a new query and use the indicated documents as feedback.
 * Note that we only use the docid portion of the indicated documents,
 * even if parts were given.  This is because, at the moment, nothing
 * in the feedback area understands parts.
 *
 ******************************************************************/

/* HACK. to be reconciled when libinter is re-implemented:
   If second argument is non-NULL, it is treated as a SM_BUF pointer
   to an in-core copy of the query.  This is a non-standard
   invocation of this procedure, intended for client-server purposes */
int
run_with_feedback(is, in_core_query, inst)
INTER_STATE *is;
SM_BUF *in_core_query;
int inst;
{
    int status;
    QUERY_VECTOR query_vec, fdbk_query;
    TR_VEC tr_vec;
    long command, i;
    long temp_did;
    RETRIEVAL local_retrieval;
    STATIC_INFO *ip;
    SM_INDEX_TEXTDOC query_index_textdoc;
    long qid;

    PRINT_TRACE (2, print_string, "Trace: entering run_with_feedback");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.run_with_feedback");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (UNDEF == next_vecid_1 ((char *) NULL, &qid, ip->next_vecid_1_inst))
	return (UNDEF);
    query_index_textdoc.id_num.id = qid;
    if (1 != (status = ip->get_query.ptab->proc (in_core_query,
                                              &query_index_textdoc,
                                              ip->get_query.inst))) {
        return (status);
    }
    is->query_text.size = 0;
    is->query_text.buf = query_index_textdoc.doc_text;
    is->query_text.end = query_index_textdoc.textloc_doc.end_text;
    is->current_query = qid;

    if (1 != (status = ip->preparse_query.ptab->proc (&is->query_text,
                                               &query_index_textdoc.mem_doc,
                                               ip->preparse_query.inst))) {
        return (status);
    }
    if (1 != (status = ip->index_query.ptab->proc (&query_index_textdoc,
                                              &ip->qvec,
					      ip->index_query.inst))) {
        return (status);
    }

    is->retrieval.tr->num_tr = 0;
    is->retrieval.query->vector = (char *) &ip->qvec;
    is->retrieval.query->qid = qid;

    if (is->retrieval.query->vector == (char *) NULL) {
        if (UNDEF == add_buf_string ("No query to run feedback on.",
                                     &is->err_buf))
            return (UNDEF);
        return (0);
    }

    if (NULL == (tr_vec.tr = (TR_TUP *) malloc
                 ((unsigned) is->num_command_line * sizeof (TR_TUP) )))
	return UNDEF;
    tr_vec.num_tr = 0;
    tr_vec.qid = query_vec.qid;

    for (i=0,command = 1; command < is->num_command_line; command++,i++) {
        temp_did = atol (is->command_line[command]);

	tr_vec.tr[i].did = temp_did;
	tr_vec.tr[i].rank = i;
	tr_vec.tr[i].action = 0;
	tr_vec.tr[i].rel = 1;
	tr_vec.tr[i].iter = 0;
	tr_vec.tr[i].sim = 0.5;
	tr_vec.num_tr++;
    }
    
    /* Restore tr_vec to be sorted in increasing did order */
    qsort ((char *) tr_vec.tr,
	   (int) tr_vec.num_tr,
	   sizeof (TR_TUP), compare_tr_did);

    /* Create feedback query */
    local_retrieval = is->retrieval;
    local_retrieval.tr = &tr_vec;
    if (UNDEF == ip->p_feedback.ptab->proc (&local_retrieval,
                                        &fdbk_query,
                                        ip->p_feedback.inst))
        return (UNDEF);

    if (fdbk_query.vector != NULL)
        is->retrieval.query->vector = fdbk_query.vector;
    
    Free( tr_vec.tr );

    status = run_retrieve(is, inst);
    PRINT_TRACE (2, print_string, "Trace: leaving run_with_feedback");

    return (status);
}


/* HACK. to be reconciled when libinter is re-implemented:
   If second argument is non-NULL, it is treated as a SM_BUF pointer
   to an in-core copy of the query.  This is a non-standard
   invocation of this procedure, intended for client-server purposes */
int
run_new (is, in_core_query, inst)
INTER_STATE *is;
SM_BUF *in_core_query;
int inst;
{
    STATIC_INFO *ip;
    SM_INDEX_TEXTDOC query_index_textdoc;
    int status;
    long qid;

    PRINT_TRACE (2, print_string, "Trace: entering run_new");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.run_new");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (UNDEF == next_vecid_1 ((char *) NULL, &qid, ip->next_vecid_1_inst))
	return (UNDEF);
    query_index_textdoc.id_num.id = qid;
    if (1 != (status = ip->get_query.ptab->proc (in_core_query,
                                              &query_index_textdoc,
                                              ip->get_query.inst))) {
        return (status);
    }
    is->query_text.size = 0;
    is->query_text.buf = query_index_textdoc.doc_text;
    is->query_text.end = query_index_textdoc.textloc_doc.end_text;

    if (1 != (status = ip->preparse_query.ptab->proc (&is->query_text,
                                               &query_index_textdoc.mem_doc,
                                               ip->preparse_query.inst))) {
        return (status);
    }
    if (1 != (status = ip->index_query.ptab->proc (&query_index_textdoc,
                                              &ip->qvec,
					      ip->index_query.inst))) {
        return (status);
    }

    is->retrieval.tr->num_tr = 0;
    is->retrieval.query->vector = (char *) &ip->qvec;
    is->retrieval.query->qid = qid;

    status = run_retrieve (is, inst);

    PRINT_TRACE (2, print_string, "Trace: leaving run_new");

    return (status);
}

/******************************************************************
 *
 * A comparison routine that operates on simple docids.
 *
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

/******************************************************************
 * A comparison routine that operates on relevance and docids.
 ******************************************************************/
static int
compare_tr_rel_did( t1, t2 )
TR_TUP *t1, *t2;
{
    if (t1->rel > t2->rel)
        return -1;
    if (t1->rel < t2->rel)
        return 1;

    if (t1->did < t2->did)
        return -1;
    if (t1->did > t2->did)
        return 1;

    return 0;
}
