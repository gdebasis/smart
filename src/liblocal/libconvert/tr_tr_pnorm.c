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
 *2 rerank_pnorm (in_tr_file, out_tr_file, inst)
 *3 char *in_tr_file;
 *3 char *out_tr_file;
 *3 int inst;
 *4 init_rerank_pnorm (spec, unused)
 *5   "tr_filter.in_tr_file"
 *5   "tr_filter.in_tr_file.rmode"
 *5   "tr_filter.out_tr_file"
 *5   "tr_filter.trace"
 *4 close_rerank_pnorm (inst)

 *7 This should go somewhere in libretrieve.
 *7 Rerank the top-ranked documents using pnorm queries.
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
#include "pnorm_vector.h"
#include "local_functions.h"

static char *default_tr_file;
static long tr_mode;
static char *out_tr_file;
static long top;
static float coeff;

static char *qvec_file;
static long read_mode;

static SPEC_PARAM spec_args[] = {
    {"tr_filter.in_tr_file",	getspec_dbfile,	(char *) &default_tr_file},
    {"tr_filter.in_tr_file.rmode", getspec_filemode,	(char *) &tr_mode},
    {"tr_filter.out_tr_file",	getspec_dbfile,	(char *) &out_tr_file},
    {"tr_filter.top_wanted",	getspec_long,	(char *) &top},
    {"tr_filter.factor",	getspec_float,	(char *) &coeff},
    {"pnorm.query_file",	getspec_dbfile,	(char *) &qvec_file},
    {"pnorm.query_file.rmode",	getspec_filemode,	(char *) &read_mode},
    TRACE_PARAM ("tr_filter.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int did_vec_inst, vv_inst, q_fd;
static int comp_sim(), comp_did();

int init_sim_pvec_pvec(), sim_pvec_pvec(), close_sim_pvec_pvec();


int
init_rerank_pnorm (spec, unused)
SPEC *spec;
char *unused;
{
    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_rerank_pnorm");

    if (UNDEF == (did_vec_inst = init_did_vec (spec, (char *) NULL)) ||
	UNDEF == (vv_inst = init_sim_pvec_pvec(spec, (char *)NULL)))
	return (UNDEF);
    if (UNDEF == (q_fd = open_pnorm(qvec_file, read_mode)))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_rerank_pnorm");

    return (0);
}

int
rerank_pnorm (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
    int in_fd, out_fd, i;
    float newsim;
    VEC dvec;
    PNORM_VEC qvec;
    PVEC_VEC_PAIR vpair;
    TR_VEC tr_vec;

    PRINT_TRACE (2, print_string, "Trace: entering rerank_pnorm");

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
	if (UNDEF == seek_pnorm(q_fd, &qvec) ||
	    UNDEF == read_pnorm(q_fd, &qvec))
	    return(UNDEF);
	vpair.qvec = &qvec;
	for (i=0; i < top; i++) {
	    newsim = 0;
            dvec.id_num.id = tr_vec.tr[i].did; EXT_NONE(dvec.id_num.ext);
            if (UNDEF == did_vec(&(dvec.id_num.id), &dvec, did_vec_inst))
		return (UNDEF);
	    vpair.dvec = &dvec;
	    if (UNDEF == sim_pvec_pvec(&vpair, &newsim, vv_inst))
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

    PRINT_TRACE (2, print_string, "Trace: leaving rerank_pnorm");
    return (1);
}

int
close_rerank_pnorm (inst)
int inst;
{
  PRINT_TRACE (2, print_string, "Trace: entering close_rerank_pnorm");

  if (UNDEF == close_did_vec (did_vec_inst) ||
      UNDEF == close_sim_pvec_pvec(vv_inst)) 
    return (UNDEF);
  if (UNDEF == close_pnorm(q_fd))
      return(UNDEF);

  PRINT_TRACE (2, print_string, "Trace: leaving close_rerank_pnorm");
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
