#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libconvert/tr_tr_rerank.c,v 11.2 1993/02/03 16:51:43 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Take a tr file and apply a filter to get a new tr file.
 *1 local.convert.obj.tr_tr
 *2 tr_tr_rerank (in_tr_file, out_tr_file, inst)
 *3 char *in_tr_file;
 *3 char *out_tr_file;
 *3 int inst;
 *4 init_tr_tr_rerank (spec, unused)
 *5   "tr_filter.in_tr_file"
 *5   "tr_filter.in_tr_file.rmode"
 *5   "tr_filter.out_tr_file"
 *5   "tr_filter.trace"
 *4 close_tr_tr_rerank (inst)

 *7 This should go somewhere in libretrieve.
 *7 Rerank the top-ranked documents using various weighting methods.
 *7 
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
static PROC_TAB *vec_vec;
static float coeff;

static SPEC_PARAM spec_args[] = {
    {"tr_filter.in_tr_file",	getspec_dbfile,	(char *) &default_tr_file},
    {"tr_filter.in_tr_file.rmode", getspec_filemode,	(char *) &tr_mode},
    {"tr_filter.out_tr_file",	getspec_dbfile,	(char *) &out_tr_file},
    {"tr_filter.top_wanted",	getspec_long,	(char *) &top},
    {"tr_filter.vec_vec",	getspec_func,	(char *) &vec_vec},
    {"tr_filter.factor",	getspec_float,	(char *) &coeff},
    TRACE_PARAM ("tr_filter.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int did_vec_inst, qid_vec_inst, vv_inst;
static int comp_sim(), comp_did();

int
init_tr_tr_rerank (spec, unused)
SPEC *spec;
char *unused;
{
    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_tr_tr_rerank");
    if (UNDEF == (did_vec_inst = init_did_vec (spec, (char *) NULL)) ||
	UNDEF == (qid_vec_inst = init_qid_vec (spec, (char *) NULL)) ||
	UNDEF == (vv_inst = vec_vec->init_proc(spec, (char *)NULL)))
	return (UNDEF);
    PRINT_TRACE (2, print_string, "Trace: leaving init_exp_rel_doc");

    return (0);
}

int
tr_tr_rerank (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
    int in_fd, out_fd, i;
    float newsim;
    VEC dvec, qvec;
    VEC_PAIR vpair;
    TR_VEC tr_vec;

    PRINT_TRACE (2, print_string, "Trace: entering tr_tr_rerank");

    /* Open input and output files */
    if(in_file == (char *) NULL) in_file = default_tr_file;
    if(out_file == (char *) NULL) out_file = out_tr_file;
    if(UNDEF == (in_fd = open_tr_vec (in_file, tr_mode)) ||
       UNDEF == (out_fd = open_tr_vec (out_file, SRDWR | SCREATE)))
      return (UNDEF);
    while (1 == read_tr_vec (in_fd, &tr_vec)) {
	if(tr_vec.qid < global_start) continue;
	if(tr_vec.qid > global_end) break;
	qsort((char *) tr_vec.tr, tr_vec.num_tr, sizeof(TR_TUP), comp_sim);

        qvec.id_num.id = tr_vec.qid; EXT_NONE(qvec.id_num.ext);
	if (UNDEF == qid_vec(&(qvec.id_num.id), &qvec, qid_vec_inst))
	    return(UNDEF);
	vpair.vec1 = &qvec;
	for (i=0; i < top; i++) {
	    newsim = 0;
            dvec.id_num.id = tr_vec.tr[i].did; EXT_NONE(dvec.id_num.ext);
            if (UNDEF == did_vec(&(dvec.id_num.id), &dvec, did_vec_inst))
		return (UNDEF);
	    vpair.vec2 = &dvec;
	    if (UNDEF == vec_vec->proc(&vpair, &newsim, vv_inst))
		return(UNDEF);
	    tr_vec.tr[i].sim += coeff * newsim;
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

    PRINT_TRACE (2, print_string, "Trace: leaving tr_tr_rerank");
    return (1);
}

int
close_tr_tr_rerank (inst)
int inst;
{
  PRINT_TRACE (2, print_string, "Trace: entering close_tr_tr_rerank");
  if (UNDEF == close_did_vec (did_vec_inst) ||
      UNDEF == close_qid_vec (qid_vec_inst) ||
      UNDEF == vec_vec->close_proc(vv_inst)) 
    return (UNDEF);
  PRINT_TRACE (2, print_string, "Trace: leaving close_tr_tr_rerank");
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
