#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libconvert/tr_tr_rerank.c,v 11.2 1993/02/03 16:51:43 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Rerank top-ranked documents by applying a (soft) Boolean filter.
 *1 local.retrieve.q_tr.bool
 *2 q_tr_softbool (in_tr, out_res, inst)
 *3 TR_VEC *in_tr;
 *3 RESULT *out_res;
 *3 int inst;
 *4 init_q_tr_softbool (spec, unused)
 *4 close_q_tr_softbool (inst)

 *7 Check documents in input tr_vec to see if they contain all the 
 *7 specified terms within a small window. Promote documents that 
 *7 satisfy this constraint.
 *7 See text_boolvec_obj.c for the interpretation of the query vector.
 *7     all terms of ctypes 2n (single words) and 2n+1 (phrases) are OR-ed
 *7     different ctype pairs are AND-ed.

 *7 Use \sigma wt for AND, and MAX for OR operators.

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

static char *query_file;
static long query_mode;
static long top_reranked, top_saved;
static long winsize;
static float sim_ctype_wt, coeff;
static SPEC_PARAM spec_args[] = {
    {"retrieve.q_tr.query_file", getspec_dbfile, (char *) &query_file},
    {"retrieve.q_tr.query_file.rmode", getspec_filemode, (char *) &query_mode},
    {"retrieve.q_tr.top_reranked", getspec_long, (char *) &top_reranked},
    {"retrieve.q_tr.top_saved", getspec_long, (char *) &top_saved},
    {"retrieve.q_tr.window_size", getspec_long, (char *) &winsize},
    {"retrieve.q_tr.sim_ctype_weight", getspec_float, (char *) &sim_ctype_wt},
    {"retrieve.q_tr.factor", getspec_float, (char *) &coeff},
    TRACE_PARAM ("retrieve.q_tr.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int q_fd, condoc_inst, dvec_inst, idf_inst;
static TOP_RESULT *tr_buf;
static long max_tr_buf;
static int (*qd_match)();

static int match_win(), match_doc(), in_doc(), in_window(), in_filter();
static int comp_rank(), comp_sim();
int init_get_condoc(), get_condoc(), close_get_condoc();


int
init_q_tr_softbool (spec, unused)
SPEC *spec;
char *unused;
{
    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);
    PRINT_TRACE (2, print_string, "Trace: entering init_q_tr_softbool");

    if (UNDEF == (q_fd = open_vector(query_file, query_mode)))
	return(UNDEF);
    if (UNDEF == (condoc_inst = init_get_condoc(spec, (char *) NULL)) ||
	UNDEF == (dvec_inst = init_did_vec(spec, (char *) NULL)) ||
	UNDEF == (idf_inst = init_con_cw_idf(spec, "ctype.0.")))
	return (UNDEF);
    max_tr_buf = top_reranked;
    if (NULL == (tr_buf = Malloc(top_reranked, TOP_RESULT)))
	return(UNDEF);
    if (winsize > 0)
	qd_match = match_win;
    else qd_match = match_doc;

    PRINT_TRACE (2, print_string, "Trace: leaving init_q_tr_softbool");
    return (0);
}

int
close_q_tr_softbool (inst)
int inst;
{
  PRINT_TRACE (2, print_string, "Trace: entering close_q_tr_softbool");

  if (UNDEF == close_vector(q_fd) ||
      UNDEF == close_get_condoc(condoc_inst) ||
      UNDEF == close_did_vec(dvec_inst) ||
      UNDEF == close_con_cw_idf(idf_inst))
      return (UNDEF);
  Free(tr_buf);

  PRINT_TRACE (2, print_string, "Trace: leaving close_q_tr_softbool");
  return (0);
}


int
q_tr_softbool (in_tr, out_res, inst)
TR_VEC *in_tr;
RESULT *out_res;
int inst;
{
    int status, i;
    float newsim;
    VEC qvec;

    PRINT_TRACE (2, print_string, "Trace: entering q_tr_softbool");

    /* Get filter (list of concepts in qvec are to be AND-ed together */
    qvec.id_num.id = in_tr->qid; EXT_NONE(qvec.id_num.ext);
    if (UNDEF == seek_vector(q_fd, &qvec) ||
	UNDEF == read_vector(q_fd, &qvec))
	return(UNDEF);

    qsort((char *) in_tr->tr, in_tr->num_tr, sizeof(TR_TUP), comp_rank);
    for (i=0; i < top_reranked; i++) {
	if (UNDEF == (status = qd_match(&qvec, in_tr->tr[i].did, &newsim)))
	    return(UNDEF);
	if (status == 0) continue;
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

    PRINT_TRACE (2, print_string, "Trace: leaving q_tr_softbool");
    return (1);
}


static int
match_doc(qvec, did, sim)
VEC *qvec;
long did;
float *sim;
{
    long i;
    float max_wt;
    CON_WT *cwp, *end_ctype;
    VEC dvec;

    if (UNDEF == did_vec(&did, &dvec, dvec_inst))
	return(UNDEF);

    cwp = qvec->con_wtp; *sim = 0.0;
    for (i = 0; i < qvec->num_ctype; i++) {
	/* check single words in this clause */
	end_ctype = cwp + qvec->ctype_len[i];
	for (max_wt = 0.0; cwp < end_ctype; cwp++)
	    if (in_doc(cwp->con, 0, &dvec) &&
		cwp->wt > max_wt)
		max_wt = cwp->wt;
	/* check phrases in this clause */
	i++;
	end_ctype = cwp + qvec->ctype_len[i];
	for (; cwp < end_ctype; cwp++)
	    if (in_doc(cwp->con, 1, &dvec) &&
		sim_ctype_wt * cwp->wt > max_wt)
		max_wt = sim_ctype_wt * cwp->wt;
	/* add similarity for this clause */
	*sim += max_wt;
    }

    return 1;
}

static int
match_win(qvec, did, sim)
VEC *qvec;
long did;
float *sim;
{
    int status;
    long sec_no, begin_window,end_window, i;
    float max_wt, win_sim;
    CON_WT *cwp, *end_ctype;
    SM_CONDOC condoc;
    SM_CONSECT *this_sect;

    /* get_condoc might return 0 if can't find doc */
    if (1 != (status = get_condoc(&did, &condoc, condoc_inst)))
	return(status);

    /* Compute matches for each window */
    *sim = 0.0;
    for (sec_no = 0; sec_no < condoc.num_sections; sec_no++) {
	this_sect = &(condoc.sections[sec_no]);
	/* windows start at each query term */
	for (begin_window = 0; begin_window < this_sect->num_concepts; 
	     begin_window++)
	    if (in_filter(this_sect->concepts[begin_window].con, 
			  this_sect->concepts[begin_window].ctype,
			  qvec))
		break;

	while (1) {
	    win_sim = 0.0;
	    end_window = MIN(begin_window + winsize, this_sect->num_concepts);
	    cwp = qvec->con_wtp;
	    for (i = 0; i < qvec->num_ctype; i++) {
		/* check single words in this clause */
		end_ctype = cwp + qvec->ctype_len[i];
		for (max_wt = 0.0; cwp < end_ctype; cwp++)
		    if (in_window(cwp->con, 0, this_sect, 
				  begin_window, end_window) &&
			cwp->wt > max_wt) 
			max_wt = cwp->wt;
		/* check phrases in this clause */
		i++;
		end_ctype = cwp + qvec->ctype_len[i];
		for (; cwp < end_ctype; cwp++)
		    if (in_window(cwp->con, 1, this_sect, 
				  begin_window, end_window) &&
			sim_ctype_wt * cwp->wt > max_wt) 
			max_wt = sim_ctype_wt * cwp->wt;
		win_sim += max_wt;
	    }

	    if (win_sim > *sim) *sim = win_sim;
	    if (begin_window + winsize >= this_sect->num_concepts)
		break;
	    for (begin_window++; begin_window < this_sect->num_concepts; 
		 begin_window++)
		if (in_filter(this_sect->concepts[begin_window].con, 
			      this_sect->concepts[begin_window].ctype,
			      qvec))
		    break;
	}
    }

    return 1;
}

static int
in_doc(con, ctype, vec)
long con, ctype;
VEC *vec;
{
    int i;
    CON_WT *ctype_start;

    ctype_start = vec->con_wtp;
    for (i = 0; i < ctype; i++)
	ctype_start += vec->ctype_len[i];
    for (i = 0; i < vec->ctype_len[ctype]; i++)
	if (ctype_start[i].con == con)
	    return 1;
    return 0;
}

static int
in_window(con, ctype, section, beg_win, end_win)
long con, ctype;
SM_CONSECT *section;
long beg_win, end_win;
{
    int i;

    for (i = beg_win; i < end_win; i++)
	if (section->concepts[i].ctype == ctype &&
	    section->concepts[i].con == con)
	    return 1;
    return 0;
}

static int
in_filter(con, ctype, vec)
long con, ctype;
VEC *vec;
{
    int i, j;
    CON_WT *cwp;

    cwp = vec->con_wtp;
    for (i = 0; i < vec->num_ctype; i++)
	for (j = 0; j < vec->ctype_len[i]; j++, cwp++)
	    if (cwp->con == con &&
		i % 2 == ctype)	/* even ctypes = single words, 
				 * odd ctypes = phrases */
		return 1;
    return 0;
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
