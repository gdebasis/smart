#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libconvert/tr_filter.c,v 11.2 1993/02/03 16:51:43 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Take a tr file and apply a filter to get a new tr file.
 *1 local.convert.rerank.nnsim
 *2 rerank_nnsim (in_tr_file, out_tr_file, inst)
 *3 char *in_tr_file;
 *3 char *out_tr_file;
 *3 int inst;
 *4 init_rerank_nnsim (spec, unused)
 *5   "rerank.in_tr_file"
 *5   "rerank.in_tr_file.rmode"
 *5   "rerank.out_tr_file"
 *5   "rerank.trace"
 *4 close_rerank_nnsim (inst)

 *7 This should go somewhere in libretrieve.
 *7 Cluster the top ranked documents.
***********************************************************************/

#include <ctype.h>
#include "common.h"
#include "compare.h"
#include "param.h"
#include "dict.h"
#include "io.h"
#include "functions.h"
#include "local_functions.h"
#include "smart_error.h"
#include "spec.h"
#include "tr_vec.h"
#include "trace.h"
#include "vector.h"
#include "eid.h"

static char *default_tr_file;
static long tr_mode;
static char *out_tr_file;
static long top_wanted;
static float factor;
static PROC_TAB *vec_vec_ptab;
static long num_nn;
static long num_ctypes;
static char *cent_file;
static long print_flag, cent_flag;

static SPEC_PARAM spec_args[] = {
    {"rerank.in_tr_file",	getspec_dbfile,	(char *) &default_tr_file},
    {"rerank.in_tr_file.rmode",	getspec_filemode,	(char *) &tr_mode},
    {"rerank.out_tr_file",	getspec_dbfile,	(char *) &out_tr_file},
    {"rerank.top_wanted",	getspec_long,	(char *) &top_wanted},    
    {"rerank.factor",		getspec_float,	(char *) &factor},
    {"rerank.vec_vec",		getspec_func,	(char *)&vec_vec_ptab},
    {"rerank.num_nn",		getspec_long,	(char *) &num_nn},
    {"rerank.num_ctypes",	getspec_long,	(char *) &num_ctypes},
    {"rerank.cent_file",	getspec_dbfile,	(char *) &cent_file},
    {"rerank.print_stats",	getspec_bool,	(char *) &print_flag},    
    {"rerank.save_cents",	getspec_bool,	(char *) &cent_flag},    
    TRACE_PARAM ("rerank.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static char *prefix;
static long num;
static SPEC_PARAM_PREFIX pspec_args[] = {
    {&prefix, "cent_num_terms",	getspec_long,	(char *) &num},
    };
static int num_pspec_args = sizeof (pspec_args) / sizeof (pspec_args[0]);

int vec_inst, vv_inst, cent_inst;
long *num_terms;
EID_LIST neighbors;
VEC *veclist;
RESULT_TUPLE *simlist, *nn;

static int comp_rank(), comp_sim(), comp_did(), compare_sim();
static int comp_wt(), comp_con();
static void print_nn();

int
init_rerank_nnsim (spec, unused)
SPEC *spec;
char *unused;
{
    char prefix_buf[PATH_LEN];
    int i;

    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_rerank_nnsim");

    if (UNDEF == (vec_inst = init_did_vec(spec, NULL)) ||
	UNDEF == (vv_inst = vec_vec_ptab->init_proc(spec, NULL)) ||
	UNDEF == (cent_inst = init_create_cent(spec, NULL)))
	return(UNDEF);

    if (top_wanted > 0 && 
	(NULL == (veclist = Malloc(top_wanted, VEC)) ||
	 NULL == (simlist = Malloc(top_wanted*(top_wanted-1), RESULT_TUPLE)) ||
	 NULL == (nn = Malloc(top_wanted, RESULT_TUPLE))))
	return(UNDEF);
    num_nn = MIN(num_nn, top_wanted - 1);
    if (num_nn > 0 &&
	NULL == (neighbors.eid = Malloc(num_nn, EID)))
	return(UNDEF);

    if (cent_flag) {
	if (NULL == (num_terms = Malloc(num_ctypes, long)))
	    return(UNDEF);
	for (i=0; i < num_ctypes; i++) {
	    sprintf(prefix_buf, "ctype.%d.", i);
	    prefix = prefix_buf;
	    if (UNDEF == lookup_spec_prefix(spec, &pspec_args[0], num_pspec_args))
		return(UNDEF);
	    num_terms[i] = num;
	}
    }

    PRINT_TRACE (2, print_string, "Trace: leaving init_rerank_nnsim");

    return (0);
}

#define SEPARATOR "\n-------------------------------------------------------\n"

int
rerank_nnsim (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
    int in_fd, out_fd, vec_fd;
    long num_results, ctype_num_terms, i, j, k;
    float avg_sim;
    EID docid;
    CON_WT *cwp, *start_ctype;
    VEC nn_vec;
    VEC_PAIR vpair;
    TR_VEC tr_vec;

    PRINT_TRACE (2, print_string, "Trace: entering rerank_nnsim");

    /* Open input and output files */
    if (in_file == (char *) NULL) in_file = default_tr_file;
    if (out_file == (char *) NULL) out_file = out_tr_file;
    if (UNDEF == (in_fd = open_tr_vec (in_file, tr_mode)))
	return (UNDEF);
    if (UNDEF == (out_fd = open_tr_vec (out_file, SRDWR))) {
	clr_err();
	if (UNDEF == (out_fd = open_tr_vec (out_file, SRDWR|SCREATE)))
	    return(UNDEF);
    }
    vec_fd = -1;
    if (cent_flag) {
	if (UNDEF == (vec_fd = open_vector (cent_file, SRDWR))) {
	    clr_err();
	    if (UNDEF == (vec_fd = open_vector (cent_file, SRDWR|SCREATE)))
		return(UNDEF);
	}
    }

    while (1 == read_tr_vec (in_fd, &tr_vec)) {
	if(tr_vec.qid < global_start) continue;
	if(tr_vec.qid > global_end) break;

	if (print_flag) {
	    printf(SEPARATOR); 
	    printf("%ld", tr_vec.qid); 
	    printf(SEPARATOR); 
	    printf("\n");
	}

	qsort((char *) tr_vec.tr, tr_vec.num_tr, sizeof(TR_TUP), comp_rank);

	/* Get vectors for top-ranked documents */
        for (i = 0; i < top_wanted; i++) {
	    docid.id = tr_vec.tr[i].did; EXT_NONE(docid.ext);
	    if (UNDEF == did_vec(&docid, &veclist[i], vec_inst) ||
		UNDEF == save_vec(&veclist[i]))
		return(UNDEF);
	}
	/* qsort((char *)veclist, top_wanted, sizeof(VEC), compare_did); */

	/* Compute pairwise similarities */
	num_results = 0;
	for (i = 0; i < top_wanted; i++) {
	    for (j = i+1; j < top_wanted; j++) {
		simlist[num_results].qid = veclist[i].id_num;
		vpair.vec1 = &veclist[i];
		simlist[num_results].did = veclist[j].id_num;
		vpair.vec2 = &veclist[j];
		simlist[num_results].sim = 0.0001;
		if (UNDEF == vec_vec_ptab->proc(&vpair,
						&simlist[num_results].sim,
						vv_inst))
		    return(UNDEF);
		num_results++;
	    }
	}

	for (i=0; i < top_wanted; i++) {
	    k = 0;
	    /* Get nearest neighbours */
	    for (j=0; j < num_results; j++) {
		if (simlist[j].qid.id == tr_vec.tr[i].did)
		    nn[k++] = simlist[j];
		else if (simlist[j].did.id == tr_vec.tr[i].did) {
		    nn[k].qid = simlist[j].did;
		    nn[k].did = simlist[j].qid;
		    nn[k++].sim = simlist[j].sim;
		}
	    }
	    qsort((char *)nn, k, sizeof(RESULT_TUPLE), compare_sim);

	    /* Compute average similarity to nearest neighbours */
	    for (avg_sim = 0, k=0; k < num_nn; k++)
		avg_sim += nn[k].sim;
	    avg_sim /= num_nn;
	    tr_vec.tr[i].sim += factor * avg_sim;

	    if (print_flag)
		print_nn(nn, num_nn, tr_vec.tr[i].rel, tr_vec.tr);

	    /* Compute centroid */
	    if (cent_flag) {
		for (k = 0; k < num_nn; k++) {
		    neighbors.eid[k].id = nn[k].did.id;
		    EXT_NONE(neighbors.eid[k].ext);
		}
		neighbors.num_eids = num_nn;
		nn_vec.id_num.id = tr_vec.tr[i].did; 
		EXT_NONE(nn_vec.id_num.ext);
		if (UNDEF == create_cent(&neighbors, &nn_vec, cent_inst))
		    return(UNDEF);

		/* Retain best terms */
		for (k=0, start_ctype = cwp = nn_vec.con_wtp, nn_vec.num_conwt=0; 
		     k < nn_vec.num_ctype; 
		     k++) {
		    qsort((char *)cwp, nn_vec.ctype_len[k], 
			  sizeof(CON_WT), comp_wt);
		    ctype_num_terms = MIN(nn_vec.ctype_len[k], num_terms[k]);
		    qsort((char *)cwp, ctype_num_terms, sizeof(CON_WT), 
			  comp_con);
		    if (start_ctype != cwp) 
			bcopy(cwp, start_ctype, ctype_num_terms*sizeof(CON_WT));
		    cwp += nn_vec.ctype_len[k];
		    start_ctype += ctype_num_terms;
		    nn_vec.ctype_len[k] = ctype_num_terms;
		    nn_vec.num_conwt += ctype_num_terms;
		}

		if (UNDEF == seek_vector(vec_fd, &nn_vec) ||
		    UNDEF == write_vector(vec_fd, &nn_vec))
		    return(UNDEF);
	    }
	}

	/* Free vectors */
	for (i = 0; i < top_wanted; i++)
	    free_vec(&veclist[i]);	    

	/* Compute new ranks and write the result out */
	qsort((char *) tr_vec.tr, tr_vec.num_tr, sizeof(TR_TUP), comp_sim);
	for (i = 0; i < tr_vec.num_tr; i++) tr_vec.tr[i].rank = i+1;
	qsort((char *) tr_vec.tr, tr_vec.num_tr, sizeof(TR_TUP), comp_did);
	if (UNDEF == seek_tr_vec (out_fd, &tr_vec) ||
	    UNDEF == write_tr_vec (out_fd, &tr_vec))
	    return (UNDEF);
    }

    if (UNDEF == close_tr_vec (in_fd) ||
	UNDEF == close_tr_vec (out_fd))
	return (UNDEF);
    if (cent_flag &&
	UNDEF == close_vector (vec_fd))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving rerank_nnsim");
    return (1);
}

int
close_rerank_nnsim (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_rerank_nnsim");

    if (UNDEF == close_did_vec(vec_inst) ||
	UNDEF == vec_vec_ptab->close_proc(vv_inst) ||
	UNDEF == close_create_cent(cent_inst))
	return(UNDEF);

    if (top_wanted > 0) {
	Free(veclist); 
	Free(simlist);
	Free(neighbors.eid);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_rerank_nnsim");
    return (0);
}


static void
print_nn(nn, num, rel, tr)
RESULT_TUPLE *nn;
long num;
char rel;
TR_TUP *tr;
{
    int i, j;

    printf("%ld %d\t", nn->qid.id, rel);
    for (i = 0; i < num - 1; i++) {
	for (j = 0; j < top_wanted; j++) 
	    if (tr[j].did == nn[i].did.id)
		break;
	printf("%ld %6.2f %d\t", nn[i].did.id, nn[i].sim, tr[j].rel);
    }
    for (j = 0; j < top_wanted; j++) 
	if (tr[j].did == nn[i].did.id)
	    break;
    printf("%ld %6.2f %d\n", nn[i].did.id, nn[i].sim, tr[j].rel);
    return;
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
compare_sim(r1, r2)
RESULT_TUPLE *r1, *r2;
{
    if (r1->sim > r2->sim)
	return -1;
    if (r1->sim < r2->sim)
	return 1; 
    return 0;
}

static int
comp_wt(cw1, cw2)
CON_WT *cw1, *cw2;
{
    if (cw1->wt > cw2->wt)
	return -1;
    if (cw1->wt < cw2->wt)
	return 1;
    return 0;
}

static int
comp_con(cw1, cw2)
CON_WT *cw1, *cw2;
{
    if (cw1->con > cw2->con)
	return 1;
    if (cw1->con < cw2->con)
	return -1;
    return 0;
}
