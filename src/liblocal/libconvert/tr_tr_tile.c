#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libconvert/tr_tr_rerank.c,v 11.2 1993/02/03 16:51:43 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Take a tr file and the segmentation information of the top retrieved
 * documents to reorder the document ranks.
 *1 local.convert.obj.tr_tr
 *2 rerank_tile (in_tr_file, out_tr_file, inst)
 *3 char *in_tr_file;
 *3 char *out_tr_file;
 *3 int inst;
 *4 init_rerank_tile (spec, unused)
 *5   "rerank.in_tr_file"
 *5   "rerank.in_tr_file.rmode"
 *5   "rerank.out_tr_file"
 *5   "rerank.trace"
 *4 close_rerank_tile (inst)
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

/* #define VERBOSE */

#define MAX_CORR 2048

static char  *default_tr_file;
static long  tr_mode;
static char  *out_tr_file;
static long  top;
static long  winsize;
static float coeff;
static long  max_terms;
static int   doc_fd;
static long  doc_mode;
static char* doc_file;

#ifdef VERBOSE
static PROC_TAB *contok;
#endif

static SPEC_PARAM spec_args[] = {
    {"rerank.in_tr_file",	getspec_dbfile,	(char *) &default_tr_file},
    {"rerank.in_tr_file.rmode", getspec_filemode,	(char *) &tr_mode},
    {"rerank.out_tr_file",	getspec_dbfile,	(char *) &out_tr_file},
    {"rerank.top_wanted",	getspec_long,	(char *) &top},
    {"rerank.window_size",	getspec_long,	(char *) &winsize},
    {"rerank.factor",	getspec_float,	(char *) &coeff},
    {"rerank.max_terms",	getspec_long,	(char *) &max_terms},
    {"rerank.doc_file",     getspec_dbfile,    (char *) &doc_file},
    {"rerank.doc_file.rmode", getspec_filemode,(char *) &doc_mode},
#ifdef VERBOSE
    {"rerank.con_to_token",	getspec_func,	(char *) &contok},
#endif
    TRACE_PARAM ("rerank.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

typedef struct match {
    long con;
    float qwt, dwt;
    long num_ret;
    float df;
} MATCH;

static int qid_vec_inst, condoc_inst, df_inst;
#ifdef VERBOSE
static int contok_inst;
#endif

static void prune_query();
static int num_matches();
static int comp_rank(), comp_sim(), comp_did(), comp_weight(), comp_con();
static int compare_con(), compare_position(), comp_match();
int init_get_condoc(), get_condoc(), close_get_condoc();


int
init_rerank_coocc_lidf (spec, unused)
SPEC *spec;
char *unused;
{
    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_rerank_coocc_lidf");
    if (UNDEF == (qid_vec_inst = init_qid_vec(spec, (char *) NULL)) ||
	UNDEF == (condoc_inst = init_get_condoc(spec, (char *) NULL)) ||
	UNDEF == (df_inst = init_con_invfreq(spec, "ctype.0.")))
	return (UNDEF);
#ifdef VERBOSE
    if (UNDEF == (contok_inst = contok->init_proc(spec, "ctype.0.")))
	return(UNDEF);
#endif
    if (! VALID_FILE (doc_file))
        return UNDEF ;

    PRINT_TRACE (2, print_string, "Trace: leaving init_rerank_coocc_lidf");

    return (0);
}


int
rerank_coocc_lidf (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
    int    in_fd, out_fd, i;
    float  newsim;
	VEC    docvec;
    VEC    qvec;
    TR_VEC tr_vec;
    int    status;

    PRINT_TRACE (2, print_string, "Trace: entering rerank_coocc_lidf");

    /* Open input and output files */
    if (in_file == (char *) NULL) in_file = default_tr_file;
    if (out_file == (char *) NULL) out_file = out_tr_file;
    if (UNDEF == (in_fd = open_tr_vec (in_file, tr_mode)) ||
	UNDEF == (out_fd = open_tr_vec (out_file, SRDWR | SCREATE)))
		return (UNDEF);

    while (1 == read_tr_vec (in_fd, &tr_vec)) {
		if(tr_vec.qid < global_start) continue;
		if(tr_vec.qid > global_end) break;
		qsort((char *) tr_vec.tr, tr_vec.num_tr, sizeof(TR_TUP), comp_rank);

	    qvec.id_num.id = tr_vec.qid; EXT_NONE(qvec.id_num.ext);
		if (UNDEF == qid_vec(&(qvec.id_num.id), &qvec, qid_vec_inst))
	    	return(UNDEF);

		/* Revisit: Not sure we need to prune query terms for nnn queries. prune_query(&qvec); */

		for (i=0; i < top; i++) {
	    	newsim = 0;
			// Get the document vector from the id.
		    if (UNDEF == (doc_fd = open_vector (doc_file, doc_mode)))
		        return (UNDEF);

			/* Recompute the 'sim' of this tr_vec */ 
		    if (UNDEF == tileScore(&qvec, &doc_fd, &newsim))
				return(UNDEF);
	    	tr_vec.tr[i].sim += coeff * newsim;		// Revisit: is the coeff needed?
		}

		qsort((char *) tr_vec.tr, tr_vec.num_tr, sizeof(TR_TUP), comp_sim);
		for (i = 0; i < tr_vec.num_tr; i++) tr_vec.tr[i].rank = i+1;
		qsort((char *) tr_vec.tr, tr_vec.num_tr, sizeof(TR_TUP), comp_did);
		if (UNDEF == seek_tr_vec (out_fd, &tr_vec) ||
		    UNDEF == write_tr_vec (out_fd, &tr_vec))
	    	return (UNDEF);
    }

    if(UNDEF == close_tr_vec (in_fd) ||
   	   UNDEF == close_tr_vec (out_fd))
      return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving rerank_coocc_lidf");
    return (1);
}

int
close_rerank_coocc_lidf (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_rerank_coocc_lidf");

    if (UNDEF == close_qid_vec(qid_vec_inst) ||
	UNDEF == close_get_condoc(condoc_inst) ||
	UNDEF == close_con_invfreq(df_inst))
	return (UNDEF);
#ifdef VERBOSE
    if (UNDEF == contok->close_proc(contok_inst))
	return(UNDEF);
#endif
    PRINT_TRACE (2, print_string, "Trace: leaving close_rerank_coocc_lidf");
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

#define LOCAL_DF(ret, df) 1.0
#endif

/* Make use of the tiling annotation to recompute the score of this document query pair */
static int
num_matches(query, doc, sim)
VEC *query;
SM_CONDOC *doc;
float *sim;
{
    long num_match, coni, conj, sec_no, begin_window,end_window, i, j, k;
    float df, prob, min_prob, total, best_win;
    SM_CONSECT *this_sect;
    MATCH *matchlist;

    if (NULL == (matchlist = Malloc(query->ctype_len[0], MATCH)))
	return(UNDEF);

    best_win = 0;
    /* Compute num_matches for each window, and find best num_matches */
    for (sec_no = 0; sec_no < doc->num_sections; sec_no++) {
	this_sect = &(doc->sections[sec_no]);
	begin_window = 0;
	while (begin_window == 0 ||
	       begin_window + winsize/2 < this_sect->num_concepts) {
	    end_window = MIN(begin_window + winsize, this_sect->num_concepts);
	    num_match = 0;

	    /* Sort the concepts in this window by <ctype, con> */
	    qsort((char *)&this_sect->concepts[begin_window], 
		  end_window - begin_window,
		  sizeof(SM_CONLOC),
		  compare_con);

	    /* Find the matching single terms for this window */
	    /* Local df used instead of usual idf. 
	     * Formula: 10/log(1 + df/(#ret+eps)) */
	    for (i = begin_window, j = 0, k = 0; 
		 i < end_window && j < query->ctype_len[0];) {
		if (this_sect->concepts[i].ctype > 0) break;
		if (this_sect->concepts[i].con < query->con_wtp[j].con)
		    i++;
		else if (this_sect->concepts[i].con > query->con_wtp[j].con)
		    j++;
		else {
		    matchlist[num_match].con = query->con_wtp[j].con;
		    if (UNDEF == con_invfreq(&(matchlist[num_match].con),
					     &df, df_inst))
			return(UNDEF);
		    matchlist[num_match].qwt = query->con_wtp[j].wt;
		    matchlist[num_match].dwt = 1;
		    matchlist[num_match].df = df;
		    while (this_query[k].qid == query->id_num.id) {
			if (query->con_wtp[j].con == this_query[k].con1) {
			    matchlist[num_match].num_ret = this_query[k].count1;
			    matchlist[num_match].qwt *= 
				LOCAL_DF(this_query[k].count1, df);

			    break;
			}
			if (query->con_wtp[j].con == this_query[k].con2) {
			    matchlist[num_match].num_ret = this_query[k].count2;
			    matchlist[num_match].qwt *= 
				LOCAL_DF(this_query[k].count2, df);
			    break;
			}
			k++;
		    }
		    /*		    assert(this_query[k].qid == query->id_num.id);*/
		    i++; j++; num_match++;
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

	    /* Sort matching terms in increasing order of num_ret */
	    qsort((char *)matchlist, num_match, sizeof(MATCH), comp_match);

	    /* Compute similarity for this window:
	     * Similarity = Wt(T1) + 
	     * Sum(i, 2, #matches) Wt(Ti) * (MIN(j, 1, i-1) P(!Ti|Tj)) */
	    total = matchlist[0].qwt * matchlist[0].dwt;
	    for (i = 1; i < num_match; i++) {
		coni = matchlist[i].con;
		min_prob = 1;
		for (j = 0; j < i; j++) {
		    conj = matchlist[j].con;
		    k = 0;
		    while (this_query[k].qid == query->id_num.id) {
			if (this_query[k].con1 == coni && 
			    this_query[k].con2 == conj) {
			    prob = 1 - (float) this_query[k].common/
					       this_query[k].count2;
			    min_prob = MIN(prob, min_prob);
			    break;
			}
			if (this_query[k].con2 == coni && 
			    this_query[k].con1 == conj) {
			    prob = 1 - (float) this_query[k].common/
					       this_query[k].count1;
			    min_prob = MIN(prob, min_prob);
			    break;
			}
			k++;
		    }
                    assert(this_query[k].qid == query->id_num.id);
		}
		total += min_prob * matchlist[i].qwt * matchlist[i].dwt;
	    }

#ifdef VERBOSE
	    if (total > best_win) {
		char *tokbuf;
		printf("%ld %ld", query->id_num.id, doc->id_num);
		for (i = 0; i < num_match; i++) {
		    if (UNDEF == contok->proc(&(matchlist[i].con),
					      &tokbuf, contok_inst))
			return(UNDEF);
		    printf("   %s %6.3f", tokbuf, matchlist[i].qwt);
		}
		printf("\t%6.3f\n", total);
	    }
#endif
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
comp_did (tr1, tr2)
TR_TUP *tr1;
TR_TUP *tr2;
{
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
/*
    if (m1->num_ret/m1->df > m2->num_ret/m2->df)
	return -1;
    if (m1->num_ret/m1->df < m2->num_ret/m2->df)
	return 1;
*/

    if (m1->num_ret > m2->num_ret) return -1;
    if (m1->num_ret < m2->num_ret) return 1;

    if (m1->qwt > m2->qwt) return -1;
    if (m1->qwt < m2->qwt) return 1;

    return 0;
}
