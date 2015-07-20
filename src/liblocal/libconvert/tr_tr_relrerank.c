#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libconvert/tr_tr_rel_rerank.c,v 11.2 1993/02/03 16:51:43 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Take a tr file and apply a filter to get a new tr file.
 *1 local.convert.obj.tr_tr
 *2 tr_tr_rel_rerank (in_tr_file, out_tr_file, inst)
 *3 char *in_tr_file;
 *3 char *out_tr_file;
 *3 int inst;
 *4 init_tr_tr_rel_rerank (spec, unused)
 *5   "tr_filter.in_tr_file"
 *5   "tr_filter.in_tr_file.rmode"
 *5   "tr_filter.out_tr_file"
 *5   "tr_filter.trace"
 *4 close_tr_tr_rel_rerank (inst)

 Reranks a tr_file bu injecting in known relevant documents. This
 is used for the robsutness analysis test of TRLM submitted
 in SIGIR 2012.
 The input is the working set size 'W' and the maximum number of true relevant
 documents required in this working set 'R'. We first set the rel fields
 of the top ranked 'W' documents to 1 and count the number of true rels
 in this working set (found). We then move out of the woking set to find
 R-found documents from the rest of the retrieved result. If we do find
 one, then we swap this found record with a pseudo-rel one in the working
 set. Thus it is ensured that the working set comprises of atmost
 R number of true relevant documents and at least W-R number of non
 relevant documents.
***********************************************************************/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "dict.h"
#include "functions.h"
#include "io.h"
#include "proc.h"
#include "smart_error.h"
#include "spec.h"
#include "tr_vec.h"
#include "trace.h"
#include "vector.h"

static char *default_tr_file;
static long tr_mode;
static char *out_tr_file;
static long top;
static long rel_wanted;
static float coeff;
static TR_TUP* tr_buff;
static int tr_buff_size = 1000;

static SPEC_PARAM spec_args[] = {
    {"tr_filter.in_tr_file",	getspec_dbfile,	(char *) &default_tr_file},
    {"tr_filter.in_tr_file.rmode", getspec_filemode,	(char *) &tr_mode},
    {"tr_filter.out_tr_file",	getspec_dbfile,	(char *) &out_tr_file},
    {"tr_filter.top_wanted",	getspec_long,	(char *) &top},
    {"tr_filter.rel_wanted",	getspec_long,	(char *) &rel_wanted},
    TRACE_PARAM ("tr_filter.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int did_vec_inst, qid_vec_inst, vv_inst;
static int comp_sim(), comp_did();

int
init_tr_tr_rel_rerank (spec, unused)
SPEC *spec;
char *unused;
{
    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

	tr_buff = (TR_TUP*)malloc (sizeof(TR_TUP) * tr_buff_size);
	if (tr_buff == NULL)
		return UNDEF;

    PRINT_TRACE (2, print_string, "Trace: entering init_tr_tr_rel_rerank");
    PRINT_TRACE (2, print_string, "Trace: leaving init_exp_rel_doc");
    return (0);
}

// Returns a pointer to a tr_tup record which is not relevant
TR_TUP* getNonRelevantDocInTop(TR_VEC* tr_vec, int top) {
	TR_TUP *tr, *trtup = tr_vec->tr;
	int num_tr = tr_vec->num_tr;

	for (tr = trtup + top - 1; tr >= trtup; tr--) {
		if (tr->rel == 0)
			return tr;
	}

	for (tr = trtup + num_tr - 1; tr >= trtup + top; tr--) {
		if (tr->rel == 0)
			return tr;
	}
	return trtup + num_tr - 1;	// fallback: return the last record (just in case) :)
}

void swap_tr(TR_TUP* this, TR_TUP* that) {
	TR_TUP tmp;
	memcpy(&tmp, this, sizeof(TR_TUP));	// tmp <- this
	memcpy(this, that, sizeof(TR_TUP));	// this <- that
	memcpy(that, &tmp, sizeof(TR_TUP));	// that <- tmp
}	

int
tr_tr_rel_rerank (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
    int in_fd, out_fd, i, rel_found;
    TR_VEC tr_vec;
	TR_TUP *tr, *nonrel_tr;

    PRINT_TRACE (2, print_string, "Trace: entering tr_tr_rel_rerank");

    /* Open input and output files */
    if (in_file == (char *) NULL) in_file = default_tr_file;
    if (out_file == (char *) NULL) out_file = out_tr_file;
    if(UNDEF == (in_fd = open_tr_vec (in_file, tr_mode)) ||
       UNDEF == (out_fd = open_tr_vec (out_file, SRDWR | SCREATE)))
      return (UNDEF);

    while (1 == read_tr_vec (in_fd, &tr_vec)) {
		rel_found = 0;
		if (tr_vec.qid < global_start) continue;
		if (tr_vec.qid > global_end) break;

		if (tr_vec.num_tr > tr_buff_size) {
			tr_buff_size = tr_vec.num_tr<<1;
			tr_buff = Realloc(tr_buff, tr_buff_size, TR_TUP);
			if (tr_buff == NULL)
				return UNDEF;
		}
		memcpy(tr_buff, tr_vec.tr, tr_vec.num_tr*sizeof(TR_TUP));
		tr_vec.tr = tr_buff;

		qsort((char *) tr_vec.tr, tr_vec.num_tr, sizeof(TR_TUP), comp_sim);

		for (i = 0; i < top; i++) {
			tr = &tr_vec.tr[i];
			if (tr->rel == 1)
				// this is a relevant document in the top working set
				rel_found++;
		}

		for (i = top; i < tr_vec.num_tr; i++) {
			if (rel_found >= rel_wanted)
				break;
			tr = &tr_vec.tr[i];
			if (tr->rel == 1) {
				rel_found++;
				nonrel_tr = getNonRelevantDocInTop(&tr_vec, top);
				// swap this record with the nonrel_tr
				swap_tr(tr, nonrel_tr);
			}
		}

		// make the top ranked documents relevant and others non relevant
		for (i = 0; i < tr_vec.num_tr; i++) {
			tr = &tr_vec.tr[i];
			tr->rel = i < top ? 1 : 0;
		}

		qsort((char *) tr_vec.tr, tr_vec.num_tr, sizeof(TR_TUP), comp_did);
		if (UNDEF == seek_tr_vec (out_fd, &tr_vec) ||
		    UNDEF == write_tr_vec (out_fd, &tr_vec))
	    	return (UNDEF);
    }

    if (UNDEF == close_tr_vec (in_fd) ||
       UNDEF == close_tr_vec (out_fd))
      return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving tr_tr_rel_rerank");
    return (1);
}

int
close_tr_tr_rel_rerank (inst)
int inst;
{
  PRINT_TRACE (2, print_string, "Trace: entering close_tr_tr_rel_rerank");
  free(tr_buff);
  PRINT_TRACE (2, print_string, "Trace: leaving close_tr_tr_rel_rerank");

  return (0);
}


int 
comp_sim (tr1, tr2)
TR_TUP *tr1;
TR_TUP *tr2;
{
    if (tr1->sim > tr2->sim) return -1;
    if (tr1->sim < tr2->sim) return 1;

    return 0;
}

int 
comp_did (tr1, tr2)
TR_TUP *tr1;
TR_TUP *tr2;
{
    if (tr1->did > tr2->did) return 1;
    if (tr1->did < tr2->did) return -1;

    return 0;
}
