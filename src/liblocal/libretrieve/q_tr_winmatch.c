#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libconvert/tr_tr_rerank.c,v 11.2 1993/02/03 16:51:43 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Rerank top-ranked documents by counting matches in local windows
 *1 local.retrieve.q_tr.coocc
 *2 q_tr_winmatch (in_tr, out_res, inst)
 *3 TR_VEC *in_tr;
 *3 RESULT *out_res;
 *3 int inst;
 *4 init_q_tr_winmatch (spec, unused)
 *4 close_q_tr_winmatch (inst)

 *7 Rerank the top-ranked documents using number of Q-D matches in local
 *7 fixed-length windows.
 *7 Only ctype 0 terms considered.
 *7 Difference with q_tr_coocc: no cooccurrence probabilities used, simple
 *7    \sigma btn is used.
***********************************************************************/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "dict.h"
#include "docindex.h"
#include "functions.h"
#include "io.h"
#include "proc.h"
#include "smart_error.h"
#include "spec.h"
#include "tr_vec.h"
#include "trace.h"
#include "vector.h"
#include "retrieve.h"

static long top_reranked, top_saved;
static long winsize;
static float coeff;
static long max_terms;
static SPEC_PARAM spec_args[] = {
    {"retrieve.q_tr.top_reranked", getspec_long, (char *) &top_reranked},
    {"retrieve.q_tr.top_saved", getspec_long, (char *) &top_saved},
    {"retrieve.q_tr.window_size", getspec_long, (char *) &winsize},
    {"retrieve.q_tr.factor", getspec_float, (char *) &coeff},
    {"retrieve.q_tr.max_terms", getspec_long, (char *) &max_terms},
    TRACE_PARAM ("retrieve.q_tr.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static long max_tr_buf;
static TOP_RESULT *tr_buf;
static int qid_vec_inst, condoc_inst;

static void prune_query();
static int num_matches();
static int comp_rank(), comp_sim(), comp_weight(), comp_con();
static int compare_con(), compare_position();
int init_get_condoc(), get_condoc(), close_get_condoc();


int
init_q_tr_winmatch (spec, unused)
SPEC *spec;
char *unused;
{
    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);
    PRINT_TRACE (2, print_string, "Trace: entering init_q_tr_winmatch");

    if (UNDEF == (qid_vec_inst = init_qid_vec(spec, (char *) NULL)) ||
	UNDEF == (condoc_inst = init_get_condoc(spec, (char *) NULL)))
	return (UNDEF);

    max_tr_buf = top_reranked;
    if (NULL == (tr_buf = Malloc(top_reranked, TOP_RESULT)))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_q_tr_winmatch");

    return (0);
}

int
q_tr_winmatch (in_tr, out_res, inst)
TR_VEC *in_tr;
RESULT *out_res;
int inst;
{
    int i;
    float newsim;
    SM_CONDOC condoc;
    VEC qvec;
    int status;

    PRINT_TRACE (2, print_string, "Trace: entering q_tr_winmatch");

    qvec.id_num.id = in_tr->qid; EXT_NONE(qvec.id_num.ext);
    if (UNDEF == qid_vec(&(qvec.id_num.id), &qvec, qid_vec_inst))
	return(UNDEF);
    prune_query(&qvec);

    qsort((char *) in_tr->tr, in_tr->num_tr, sizeof(TR_TUP), comp_rank);
    for (i=0; i < top_reranked; i++) {
	newsim = 0;
	/*BUG-FIX. get_condoc might return 0 if can't find doc */
	if (UNDEF == (status = get_condoc(&(in_tr->tr[i].did), &condoc, 
					  condoc_inst)))
	    return(UNDEF);
	if (status == 0) continue;
	if (UNDEF == num_matches(&qvec, &condoc, &newsim))
	    return(UNDEF);
	in_tr->tr[i].sim += coeff * newsim;
    }
    qsort((char *) in_tr->tr, in_tr->num_tr, sizeof(TR_TUP), comp_sim);

    if (in_tr->num_tr > top_saved) in_tr->num_tr = top_saved;
    if (in_tr->num_tr > max_tr_buf) {
	Free(tr_buf);
	max_tr_buf += in_tr->num_tr;
	if (NULL == (tr_buf = Malloc(max_tr_buf, TOP_RESULT)))
	    return(UNDEF);
    }
    for (i = 0; i < in_tr->num_tr; i++) {
	tr_buf[i].did = in_tr->tr[i].did;
	tr_buf[i].sim = in_tr->tr[i].sim;
    }
    out_res->qid = in_tr->qid;
    out_res->top_results = tr_buf;
    out_res->num_top_results = in_tr->num_tr;
    /* the remaining fields are hopefully not used */
    out_res->num_full_results = 0;
    out_res->min_top_sim = UNDEF;

    PRINT_TRACE (2, print_string, "Trace: leaving q_tr_winmatch");
    return (1);
}

int
close_q_tr_winmatch (inst)
int inst;
{
  PRINT_TRACE (2, print_string, "Trace: entering close_q_tr_winmatch");

  if (UNDEF == close_qid_vec(qid_vec_inst) ||
      UNDEF == close_get_condoc(condoc_inst))
    return (UNDEF);
  Free(tr_buf);

  PRINT_TRACE (2, print_string, "Trace: leaving close_q_tr_winmatch");
  return (0);
}


/* Keep only best query terms for computing newsim (used in re-ranking). */
static void
prune_query(query)
VEC *query;
{
    long num_selected, i;
    CON_WT *start_old, *start_new;

    query->num_conwt = 0;
    start_old = start_new = query->con_wtp;
    for (i = 0; i < query->num_ctype; i++) {
	qsort((char *)start_old, query->ctype_len[i], sizeof(CON_WT), 
	      comp_weight);
	num_selected = MIN(max_terms, query->ctype_len[i]);
	qsort((char *)start_old, num_selected, sizeof(CON_WT), comp_con);
	if (start_old != start_new) 
	    Bcopy(start_old, start_new, num_selected * sizeof(CON_WT));
	start_old += query->ctype_len[i];
	start_new += num_selected;
	query->ctype_len[i] = num_selected;
	query->num_conwt += num_selected;
    }
    return;
}


static int
num_matches(query, doc, sim)
VEC *query;
SM_CONDOC *doc;
float *sim;
{
    long sec_no, begin_window, end_window, i, j;
    float total, best_win;
    SM_CONSECT *this_sect;

    best_win = 0;
    /* Compute num_matches for each window, and find best num_matches */
    for (sec_no = 0; sec_no < doc->num_sections; sec_no++) {
	this_sect = &(doc->sections[sec_no]);
	begin_window = 0;
	while (begin_window == 0 ||
	       begin_window + winsize/2 < this_sect->num_concepts) {
	    end_window = MIN(begin_window + winsize, this_sect->num_concepts);

	    /* Sort the concepts in this window by <ctype, con> */
	    qsort((char *)&this_sect->concepts[begin_window], 
		  end_window - begin_window,
		  sizeof(SM_CONLOC),
		  compare_con);

	    /* Find the matching single terms for this window */
	    for (total = 0.0, i = begin_window, j = 0; 
		 i < end_window && j < query->ctype_len[0];) {
		if (this_sect->concepts[i].ctype > 0) break;
		if (this_sect->concepts[i].con < query->con_wtp[j].con)
		    i++;
		else if (this_sect->concepts[i].con > query->con_wtp[j].con)
		    j++;
		else {
		    total += query->con_wtp[j].wt;
		    i++; j++;
		}
	    }

	    /* Sort concepts back to text order */
	    qsort((char *)&this_sect->concepts[begin_window], 
		  end_window - begin_window,
		  sizeof(SM_CONLOC),
		  compare_position);

	    /* Update best matching window if necessary */
	    best_win = MAX(best_win, total);
	    begin_window += winsize/2;
	}
    }

    *sim = best_win;
    return 1;
}


static int 
comp_rank (tr1, tr2)
TR_TUP *tr1;
TR_TUP *tr2;
{
    if (tr1->rank > tr2->rank) return 1;
    if (tr1->rank < tr2->rank) return -1;

    return 0;
}

static int 
comp_sim (tr1, tr2)
TR_TUP *tr1;
TR_TUP *tr2;
{
    if (tr1->sim > tr2->sim) return -1;
    if (tr1->sim < tr2->sim) return 1;

    if (tr1->did > tr2->did) return 1;
    if (tr1->did < tr2->did) return -1;

    return 0;
}

static int
comp_weight(cw1, cw2)
CON_WT *cw1, *cw2;
{
    if (cw1->wt > cw2->wt) return -1;
    if (cw1->wt < cw2->wt) return 1;
    return 0;
}

static int
comp_con(cw1, cw2)
CON_WT *cw1, *cw2;
{
    if (cw1->con > cw2->con) return 1;
    if (cw1->con < cw2->con) return -1;
    return 0;
}

static int
compare_con(cl1, cl2)
SM_CONLOC *cl1, *cl2;
{
    if (cl1->ctype > cl2->ctype) return 1;
    if (cl1->ctype < cl2->ctype) return -1;

    if (cl1->con > cl2->con) return 1;
    if (cl1->con < cl2->con) return -1;

    return 0;
}

static int
compare_position(cl1, cl2)
SM_CONLOC *cl1, *cl2;
{
    if (cl1->token_num > cl2->token_num) return 1;
    if (cl1->token_num < cl2->token_num) return -1;

    return 0;
}
