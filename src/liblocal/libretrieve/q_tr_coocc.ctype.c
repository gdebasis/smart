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
 *2 q_tr_coocc (in_tr, out_res, inst)
 *3 TR_VEC *in_tr;
 *3 RESULT *out_res;
 *3 int inst;
 *4 init_q_tr_coocc (spec, unused)
 *4 close_q_tr_coocc (inst)

 *7 Rerank the top-ranked documents using number of Q-D matches.
 *7 Incorporate cooccurrence probabilities for query terms while counting
 *7 number of Q-D matches. 
 *7 Cooccurrence information is stored in a text file, and is assumed to
 *7 be sorted by Qid, <Qterm1, Qterm2>.
 *7 Best fixed-length window of document considered in Q-D match.
 *7 Changed to handle document weighting.
 *7 Both words and phrases considered during reranking.
 *7 Word matches considered first, and then phrase matches (i.e. instead 
 *7 of sorting matches purely by num_docs, sort by <ctype, num_docs>).
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

#define MAX_CORR 2048

static long top_reranked, top_saved;
static char *corr_file;
static long winsize;
static float coeff;
static long max_terms;
static long num_ctypes;
static SPEC_PARAM spec_args[] = {
    {"retrieve.q_tr.top_reranked", getspec_long, (char *) &top_reranked},
    {"retrieve.q_tr.top_saved", getspec_long, (char *) &top_saved},
    {"retrieve.q_tr.corr_file", getspec_dbfile, (char *) &corr_file},
    {"retrieve.q_tr.window_size", getspec_long, (char *) &winsize},
    {"retrieve.q_tr.factor", getspec_float, (char *) &coeff},
    {"retrieve.q_tr.max_terms", getspec_long, (char *) &max_terms},
    {"retrieve.q_tr.num_ctypes", getspec_long, (char *) &num_ctypes},
    TRACE_PARAM ("retrieve.q_tr.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static char *prefix;
static float ctype_wt;
static SPEC_PARAM_PREFIX pspec_args[] = {
    {&prefix, "sim_ctype_weight", getspec_float,   (char *) &ctype_wt},
    };
static int num_pspec_args = sizeof (pspec_args) / sizeof (pspec_args[0]);

typedef struct correlation {
    long qid;
    long con1, con2;
    long ctype1, ctype2;
    long common, count1, count2;
} CORRELATION;

typedef struct match {
    long con, ctype;
    float qwt, dwt;
    long num_docs;
} MATCH;

static long max_corr, num_corr;
static CORRELATION *corr;
static float *sim_ctype_wt;
static int qvec_inst, dvec_inst, condoc_inst;
static int (*qd_match)();
static long max_tr_buf;
static TOP_RESULT *tr_buf;

static void prune_query();
static int get_corr(), match_doc(), match_win();
static int comp_rank(), comp_sim(), comp_weight(), comp_con();
static int compare_con(), compare_position(), comp_match();
int init_get_condoc(), get_condoc(), close_get_condoc();


int
init_q_tr_coocc (spec, unused)
SPEC *spec;
char *unused;
{
    char prefix_buf[PATH_LEN];
    long i;
    FILE *fp;

    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);
    PRINT_TRACE (2, print_string, "Trace: entering init_q_tr_coocc");

    fprintf(stderr, "q_tr_coocc.sinphr.c -- sort by <ctype, num_docs>\n");

    if (UNDEF == (qvec_inst = init_qid_vec(spec, (char *) NULL)) ||
	UNDEF == (dvec_inst = init_did_vec(spec, (char *) NULL)) ||
	UNDEF == (condoc_inst = init_get_condoc(spec, (char *) NULL)))
	return (UNDEF);

    max_corr = MAX_CORR;
    if (NULL == (corr = Malloc(max_corr, CORRELATION)))
	return(UNDEF);
    max_tr_buf = top_reranked;
    if (NULL == (tr_buf = Malloc(top_reranked, TOP_RESULT)))
	return(UNDEF);

    /* Read in correlation information */
    if (NULL == (fp = fopen(corr_file, "r")))
	return(UNDEF);
    get_corr(fp);
    fclose(fp);

    if (winsize > 0)
	qd_match = match_win;
    else qd_match = match_doc;

    if (NULL == (sim_ctype_wt = Malloc(num_ctypes, float)))
	return(UNDEF);
    for (i = 0; i < num_ctypes; i++) {
	sprintf(prefix_buf, "ctype.%ld.", i);
	prefix = prefix_buf;
	if (UNDEF == lookup_spec_prefix(spec, &pspec_args[0], num_pspec_args))
	    return(UNDEF);
	sim_ctype_wt[i] = ctype_wt;
    }

    PRINT_TRACE (2, print_string, "Trace: leaving init_q_tr_coocc");
    return (0);
}

int
q_tr_coocc (in_tr, out_res, inst)
TR_VEC *in_tr;
RESULT *out_res;
int inst;
{
    int i;
    float newsim;
    VEC qvec;
    CORRELATION *this_query;
    int status;

    PRINT_TRACE (2, print_string, "Trace: entering q_tr_coocc");

    qvec.id_num.id = in_tr->qid; EXT_NONE(qvec.id_num.ext);
    if (UNDEF == qid_vec(&(qvec.id_num.id), &qvec, qvec_inst))
	return(UNDEF);
    prune_query(&qvec);
    for (this_query = corr; 
	 this_query < corr + num_corr && this_query->qid != qvec.id_num.id;
	 this_query++);
    if (this_query < corr + num_corr) {
	qsort((char *) in_tr->tr, in_tr->num_tr, sizeof(TR_TUP), comp_rank);
	for (i=0; i < top_reranked; i++) {
	    newsim = 0;
	    if (UNDEF == (status = qd_match(&qvec, in_tr->tr[i].did, 
					    this_query, &newsim)))
		return(UNDEF);
	    if (status == 0) continue;
	    in_tr->tr[i].sim += coeff * newsim;
	}
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

    PRINT_TRACE (2, print_string, "Trace: leaving q_tr_coocc");
    return (1);
}

int
close_q_tr_coocc (inst)
int inst;
{
  PRINT_TRACE (2, print_string, "Trace: entering close_q_tr_coocc");

  if (UNDEF == close_qid_vec(qvec_inst) ||
      UNDEF == close_did_vec(dvec_inst) ||
      UNDEF == close_get_condoc(condoc_inst))
    return (UNDEF);
  Free(corr); Free(tr_buf); Free(sim_ctype_wt);

  PRINT_TRACE (2, print_string, "Trace: leaving close_q_tr_coocc");
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
get_corr(fp)
FILE *fp;
{
    CORRELATION *cptr;

    num_corr = 0;
    cptr = corr;
    while (EOF != fscanf(fp, "%ld\t%ld %ld\t%ld %ld\t%ld\t%ld\t%ld\n",
			 &cptr->qid, &cptr->ctype1, &cptr->con1, 
			 &cptr->ctype2, &cptr->con2, 
			 &cptr->common, &cptr->count1, &cptr->count2)) {
	num_corr++;
	cptr++;
	if (num_corr == max_corr) {
	    max_corr *= 2;
	    if (NULL == (corr = Realloc(corr, max_corr, CORRELATION)))
		return(UNDEF);
	    cptr = corr + num_corr;
	}
    }
    return 1;
}


static int
match_doc(query, did, this_query, sim)
VEC *query;
long did;
CORRELATION *this_query;
float *sim;
{
    long num_match, coni, conj, ctype, i, j, k;
    float prob, min_prob, total;
    CON_WT *q_start_ctype, *d_start_ctype;
    VEC dvec;
    MATCH *matchlist;

    if (UNDEF == did_vec(&did, &dvec, dvec_inst))
        return(UNDEF);

    if (NULL == (matchlist = Malloc(query->num_conwt, MATCH)))
	return(UNDEF);

    /* Find the matching terms for this doc */
    num_match = 0;
    q_start_ctype = query->con_wtp; d_start_ctype = dvec.con_wtp;
    for (ctype = 0, k = 0; 
	 ctype < query->num_ctype && ctype < dvec.num_ctype; 
	 q_start_ctype += query->ctype_len[ctype],
	     d_start_ctype += dvec.ctype_len[ctype], ctype++) {
	for (i = 0, j = 0; 
	     i < dvec.ctype_len[ctype] && j < query->ctype_len[ctype];) {
	    if (d_start_ctype[i].con < q_start_ctype[j].con)
		i++;
	    else if (d_start_ctype[i].con > q_start_ctype[j].con)
		j++;
	    else {
		matchlist[num_match].con = q_start_ctype[j].con;
		matchlist[num_match].ctype = ctype;
		matchlist[num_match].qwt = q_start_ctype[j].wt;
		matchlist[num_match].dwt = d_start_ctype[i].wt;
		while (this_query[k].qid == query->id_num.id) {
		    if (q_start_ctype[j].con == this_query[k].con1 &&
			ctype == this_query[k].ctype1) {
			matchlist[num_match].num_docs = this_query[k].count1;
			break;
		    }
		    if (q_start_ctype[j].con == this_query[k].con2 &&
			ctype == this_query[k].ctype2) {
			matchlist[num_match].num_docs = this_query[k].count2;
			break;
		    }
		    k++;
		}
		assert(this_query[k].qid == query->id_num.id);
		i++; j++; num_match++;
	    }
	}
    }

    /* Sort matching terms in increasing order of num_docs */
    qsort((char *)matchlist, num_match, sizeof(MATCH), comp_match);

    /* Compute similarity for this window:
     * Similarity = Wt(T1) + 
     * Sum(i, 2, #matches) Wt(Ti) * (MIN(j, 1, i-1) P(!Ti|Tj)) */
    total = matchlist[0].qwt * matchlist[0].dwt * sim_ctype_wt[matchlist[0].ctype];
    for (i = 1; i < num_match; i++) {
	coni = matchlist[i].con;
	min_prob = 1;
	for (j = 0; j < i; j++) {
	    conj = matchlist[j].con;
	    k = 0;
	    while (this_query[k].qid == query->id_num.id) {
		if (this_query[k].con1 == coni && 
		    this_query[k].ctype1 == matchlist[i].ctype && 
		    this_query[k].con2 == conj &&
		    this_query[k].ctype2 == matchlist[j].ctype) {
		    prob = 1 - (float) this_query[k].common/
			this_query[k].count2;
		    min_prob = MIN(prob, min_prob);
		    break;
		}
		if (this_query[k].con2 == coni && 
		    this_query[k].ctype2 == matchlist[i].ctype && 
		    this_query[k].con1 == conj &&
		    this_query[k].ctype1 == matchlist[j].ctype) {
		    prob = 1 - (float) this_query[k].common/
			this_query[k].count1;
		    min_prob = MIN(prob, min_prob);
		    break;
		}
		k++;
	    }
	    assert(this_query[k].qid == query->id_num.id);
	}
	total += min_prob * matchlist[i].qwt * matchlist[i].dwt * 
	    sim_ctype_wt[matchlist[i].ctype];
    }

    *sim = total;
    Free(matchlist);
    return 1;
}


static int
match_win(query, did, this_query, sim)
VEC *query;
long did;
CORRELATION *this_query;
float *sim;
{
    int status;
    long num_match, coni, conj, sec_no, begin_window, end_window, ctype, tf;
    long i, j, k;
    float prob, min_prob, total, best_win;
    SM_CONDOC doc;
    SM_CONSECT *this_sect;
    CON_WT *q_start_ctype;
    MATCH *matchlist;

    /*BUG-FIX. get_condoc might return 0 if can't find doc */
    if (1 != (status = get_condoc(&did, &doc, condoc_inst)))
	return(status);

    if (NULL == (matchlist = Malloc(query->num_conwt, MATCH)))
	return(UNDEF);

    best_win = 0;
    /* Compute num_matches for each window, and find best num_matches */
    for (sec_no = 0; sec_no < doc.num_sections; sec_no++) {
	this_sect = &(doc.sections[sec_no]);
	begin_window = 0;
	while (begin_window == 0 ||
	       begin_window + winsize/2 < this_sect->num_concepts) {
	    end_window = MIN(begin_window + winsize, this_sect->num_concepts);

	    /* Sort the concepts in this window by <ctype, con> */
	    qsort((char *)&this_sect->concepts[begin_window], 
		  end_window - begin_window,
		  sizeof(SM_CONLOC),
		  compare_con);

	    /* Find the matching words and phrases for this window */
	    num_match = 0;
	    q_start_ctype = query->con_wtp;
	    for (ctype = 0, i = begin_window, k = 0; 
		 ctype < query->num_ctype && i < end_window;
		 q_start_ctype += query->ctype_len[ctype], ctype++) {    
		for (j = 0; 
		     i < end_window && this_sect->concepts[i].ctype <= ctype &&
		     j < query->ctype_len[ctype];) {
		    if (this_sect->concepts[i].ctype < ctype ||
			this_sect->concepts[i].con < q_start_ctype[j].con)
			i++;
		    else if (this_sect->concepts[i].con > q_start_ctype[j].con)
			j++;
		    else {
			matchlist[num_match].con = q_start_ctype[j].con;
			matchlist[num_match].ctype = ctype;
			matchlist[num_match].qwt = q_start_ctype[j].wt;
			for (tf = 0; 
			     i < end_window &&
			     this_sect->concepts[i].ctype == ctype &&
			     this_sect->concepts[i].con == q_start_ctype[j].con;
			     i++, tf++);
			matchlist[num_match].dwt = log(log((double)tf)+1) + 1;
			while (this_query[k].qid == query->id_num.id) {
			    if (q_start_ctype[j].con == this_query[k].con1 &&
				ctype == this_query[k].ctype1) {
				matchlist[num_match].num_docs = this_query[k].count1;
				break;
			    }
			    if (q_start_ctype[j].con == this_query[k].con2) {
				matchlist[num_match].num_docs = this_query[k].count2;
				break;
			    }
			    k++;
			}
			assert(this_query[k].qid == query->id_num.id);
			j++; num_match++;
		    }
		}
	    }

	    /* Sort concepts back to text order */
	    qsort((char *)&this_sect->concepts[begin_window], 
		  end_window - begin_window,
		  sizeof(SM_CONLOC),
		  compare_position);

	    if (num_match == 0) {
		begin_window += winsize/2;
		continue;
	    }

	    /* Sort matching terms in increasing order of num_docs */
	    qsort((char *)matchlist, num_match, sizeof(MATCH), comp_match);

	    /* Compute similarity for this window:
	     * Similarity = Wt(T1) + 
	     * Sum(i, 2, #matches) Wt(Ti) * (MIN(j, 1, i-1) P(!Ti|Tj)) */
	    total = matchlist[0].qwt * matchlist[0].dwt * sim_ctype_wt[matchlist[0].ctype];
	    for (i = 1; i < num_match; i++) {
		coni = matchlist[i].con;
		min_prob = 1;
		for (j = 0; j < i; j++) {
		    conj = matchlist[j].con;
		    k = 0;
		    while (this_query[k].qid == query->id_num.id) {
			if (this_query[k].con1 == coni && 
			    this_query[k].ctype1 == matchlist[i].ctype && 
			    this_query[k].con2 == conj &&
			    this_query[k].ctype2 == matchlist[j].ctype) {
			    prob = 1 - (float) this_query[k].common/
				this_query[k].count2;
			    min_prob = MIN(prob, min_prob);
			    break;
			}
			if (this_query[k].con2 == coni && 
			    this_query[k].ctype2 == matchlist[i].ctype && 
			    this_query[k].con1 == conj &&
			    this_query[k].ctype1 == matchlist[j].ctype) {
			    prob = 1 - (float) this_query[k].common/
					       this_query[k].count1;
			    min_prob = MIN(prob, min_prob);
			    break;
			}
			k++;
		    }
		    assert(this_query[k].qid == query->id_num.id);
		}
		total += min_prob * matchlist[i].qwt * matchlist[i].dwt *
		    sim_ctype_wt[matchlist[i].ctype];
	    }

	    /* Update best matching window if necessary */
	    best_win = MAX(best_win, total);
	    begin_window += winsize/2;
	}
    }

    *sim = best_win;

    Free(matchlist);
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

static int
comp_match(m1, m2)
MATCH *m1, *m2;
{
    if (m1->ctype > m2->ctype) return 1;
    if (m1->ctype < m2->ctype) return -1;

    if (m1->num_docs > m2->num_docs) return 1;
    if (m1->num_docs < m2->num_docs) return -1;

    if (m1->qwt > m2->qwt) return -1;
    if (m1->qwt < m2->qwt) return 1;

    return 0;
}

