#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libinter/set_query.c,v 11.2 1993/02/03 16:51:26 smart Exp $";
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
#include "inter.h"
#include "textloc.h"
#include "sm_display.h"
#include "docindex.h"
#include "smart_error.h"
#include "inst.h"
#include "eid.h"
#include "trace.h"
#include "vector.h"

static char *tr_file;
static long tr_mode;
static PROC_TAB *get_named_query_ptab;

static SPEC_PARAM spec_args[] = {
    {"retrieve.tr_file",         getspec_dbfile,   (char *) &tr_file},
    {"retrieve.tr_file.rmode",   getspec_filemode, (char *) &tr_mode},
    {"inter.query.named_get_text",   getspec_func,    (char *) &get_named_query_ptab},
    TRACE_PARAM ("inter.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    int valid_query;
    int tr_fd;
    TR_TUP *saved_tr_tup;
    int num_saved_tr_tup;
    int qid_vec_inst;

    PROC_INST get_named_query;
    VEC qvec;

    int title_inst;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_set_query (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_set_query");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];

    if (UNDEF == (ip->qid_vec_inst = init_qid_vec (spec, (char *) NULL))) {
        clr_err();
        ip->valid_query = 0;
        return (0);
    }
    ip->tr_fd = UNDEF;
    if (VALID_FILE (tr_file))
        ip->tr_fd = open_tr_vec (tr_file, tr_mode);

    /* Reserve space for top-ranked list */
    ip->num_saved_tr_tup = 20;
    if (NULL == (ip->saved_tr_tup = (TR_TUP *)
                 malloc ((unsigned) ip->num_saved_tr_tup * sizeof (TR_TUP))))
        return (UNDEF);

    if (UNDEF == (ip->title_inst = init_show_titles (spec, (char *) NULL)))
        return (UNDEF);

    ip->get_named_query.ptab = get_named_query_ptab;
    if (UNDEF == (ip->get_named_query.inst =
                  ip->get_named_query.ptab->init_proc (spec, "query.")))
	return (UNDEF);

    ip->valid_query = 1;
    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_set_query");

    return (0);
}


int
set_query (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    long qid;
    EID eqid;
    SM_INDEX_TEXTDOC textdoc;

    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering set_query");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.set_query");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (is->num_command_line < 2 || (0 == (qid = atol (is->command_line[1])))){
        if (UNDEF == add_buf_string ("No query specified\n", &is->err_buf))
            return (UNDEF);
        return (0);
    }
    if (ip->valid_query == 0) {
        if (UNDEF == add_buf_string ("Query file not opened\n", &is->err_buf))
            return (UNDEF);
        return (0);
    }

    /* Get the top docs for this query */
    is->retrieval.tr->num_tr = 0;
    is->retrieval.tr->qid = qid;
    if (ip->tr_fd != UNDEF &&
        1 == seek_tr_vec (ip->tr_fd, is->retrieval.tr) &&
        1 == read_tr_vec (ip->tr_fd, is->retrieval.tr)) {
        /* Must save a copy of the top docs in local memory */
        if (is->retrieval.tr->num_tr > ip->num_saved_tr_tup) {
            ip->num_saved_tr_tup = is->retrieval.tr->num_tr;
            if (NULL == (ip->saved_tr_tup = (TR_TUP *)
                         realloc ((char *) ip->saved_tr_tup,
                                  (unsigned) ip->num_saved_tr_tup * 
                                  sizeof (TR_TUP))))
                return (UNDEF);
        }
        (void) bcopy ((char *) is->retrieval.tr->tr,
                      (char *) ip->saved_tr_tup,
                      (int) is->retrieval.tr->num_tr * sizeof (TR_TUP));
        is->retrieval.tr->tr = ip->saved_tr_tup;
    }

    /* Prepare title listing of top docs */
    if (UNDEF == inter_prepare_titles (is, (char *) NULL, ip->title_inst))
        return (UNDEF);

    /* Get the text of the query */
    eqid.id = qid;
    EXT_NONE(eqid.ext);
    if (UNDEF == ip->get_named_query.ptab->proc (&qid, &textdoc,
						 ip->get_named_query.inst))
	return (UNDEF);
    is->query_text.size = 0;
    is->query_text.buf = textdoc.doc_text;
    is->query_text.end = textdoc.textloc_doc.end_text - 
	textdoc.textloc_doc.begin_text;

    /* Get actual query vector (from either pre-indexed vectors, or by
       re-indexing on the fly */
    if (1 != qid_vec (&qid, &ip->qvec, ip->qid_vec_inst)) {
        if (UNDEF == add_buf_string ("Can't find query specified\n",
                                     &is->err_buf))
            return (UNDEF);
        return (0);
    }

    is->retrieval.query->vector = (char *) &ip->qvec;
    is->retrieval.query->qid = qid;

    PRINT_TRACE (2, print_string, "Trace: leaving set_query");
    
    return (1);
}
        
int
close_set_query (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_set_query");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.set_query");
        return (UNDEF);
    }
    ip  = &info[inst];

    ip->valid_info--;

    if (ip->valid_query == 0)
        return (0);

    if (UNDEF == close_qid_vec (ip->qid_vec_inst))
        return (UNDEF);
    if (UNDEF != ip->tr_fd && UNDEF == close_tr_vec (ip->tr_fd))
        return (UNDEF);

    (void) free ((char *) ip->saved_tr_tup);

    if (UNDEF == close_show_titles (ip->title_inst))
        return (UNDEF);

    if (UNDEF == ip->get_named_query.ptab->close_proc (ip->get_named_query.inst))
	return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_set_query");
    return (0);
}

