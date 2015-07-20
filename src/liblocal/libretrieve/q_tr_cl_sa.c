#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libconvert/tr_filter.c,v 11.2 1993/02/03 16:51:43 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Compute similarities between a query and a given set of docs
 *1 local.retrieve.q_tr.cluster_selall
 *2 q_tr_cl_sa (in_tr, out_res, inst)
 *3 TR_VEC *in_tr;
 *3 RESULT *out_res;
 *3 int inst;
 *4 init_q_tr_cl_sa (spec, unused)
 *4 close_q_tr_cl_sa (inst)

 *7 Cluster top ranked documents (similar to tr_tr_rerank.cluster-nosel.c)
 *7 and write out cluster centroid vectors to file.
 *7 Also, only the documents that belong to clusters are included in the out 
 *7 results. 
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

typedef struct {
    long num;
    EID *eid;
    VEC *vec;
} CLUSTER;

static long top_reranked, top_saved;
static float coeff;
static PROC_TAB *vec_vec_ptab;
static float cluster_sim;
static long num_ctypes;
static long cent_flag;
static char *cent_file;

static SPEC_PARAM spec_args[] = {
    {"retrieve.q_tr.top_reranked", getspec_long, (char *) &top_reranked},
    {"retrieve.q_tr.top_saved", getspec_long, (char *) &top_saved},
    {"retrieve.q_tr.factor", getspec_float, (char *) &coeff},
    {"retrieve.q_tr.vec_vec", getspec_func, (char *) &vec_vec_ptab},
    {"retrieve.q_tr.cluster_sim", getspec_float, (char *) &cluster_sim},
    {"retrieve.q_tr.num_ctypes", getspec_long, (char *) &num_ctypes},
    {"retrieve.q_tr.cent_flag",	getspec_bool, (char *) &cent_flag},
    {"retrieve.q_tr.cent_file",	getspec_dbfile, (char *) &cent_file},
    TRACE_PARAM ("retrieve.q_tr.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static char *prefix;
static long num;
static SPEC_PARAM_PREFIX pspec_args[] = {
    {&prefix, "cent_num_terms",	getspec_long,	(char *) &num},
    };
static int num_pspec_args = sizeof (pspec_args) / sizeof (pspec_args[0]);

int did_vec_inst, vv_inst, gen_cent_inst, save_cent_inst, print_inst;  
int cent_fd;
long *num_terms;
long max_tr_buf;
VEC *veclist;
RESULT_TUPLE *simlist;
CLUSTER *cluster;
TOP_RESULT *tr_buf;

static int generate_clusters();
static int comp_rank(), comp_sim(), comp_did(), comp_con(), comp_wt();


int
init_q_tr_cl_sa (spec, unused)
SPEC *spec;
char *unused;
{
    char prefix_buf[PATH_LEN];
    int i;

    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);
    PRINT_TRACE (2, print_string, "Trace: entering init_q_tr_cl_sa");

    if (UNDEF == (did_vec_inst = init_did_vec(spec, NULL)) ||
	UNDEF == (vv_inst = vec_vec_ptab->init_proc(spec, NULL)) ||
	UNDEF == (gen_cent_inst = init_create_cent(spec, "gen.cluster.")) ||
	UNDEF == (save_cent_inst = init_create_cent(spec, "save.cluster.")) ||
	UNDEF == (print_inst = init_print_vec_dict(spec, NULL)))
	return(UNDEF);

    if (top_reranked <= 0) {
	set_error(SM_ILLPA_ERR, "top_wanted", "init_q_tr_cl_sa");
	return(UNDEF);
    }
    if (NULL == (veclist = Malloc(top_reranked, VEC)) ||
	NULL == (simlist = Malloc((top_reranked*(top_reranked-1))/2, 
				  RESULT_TUPLE)) ||
	NULL == (cluster = Malloc(top_reranked, CLUSTER)))
	return(UNDEF);
    for (i = 0; i < top_reranked; i++)
	if (NULL == (cluster[i].eid = Malloc(top_reranked, EID)))
	    return(UNDEF);
    max_tr_buf = top_reranked;
    if (NULL == (tr_buf = Malloc(top_reranked, TOP_RESULT)))
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
	if (UNDEF == (cent_fd = open_vector (cent_file, SRDWR))) {
	    clr_err();
	    if (UNDEF == (cent_fd = open_vector (cent_file, SRDWR|SCREATE)))
		return(UNDEF);
	}
    }

    PRINT_TRACE (2, print_string, "Trace: leaving init_q_tr_cl_sa");
    return (0);
}

#define DEBUG
#define SEPARATOR "\n-------------------------------------------------------\n"

int
q_tr_cl_sa (in_tr, out_res, inst)
TR_VEC *in_tr;
RESULT *out_res;
int inst;
{
#ifdef DEBUG
    char *rel;
#endif
    long num_results, ctype_num_terms, i, j, k;
    EID docid;
    EID_LIST elist;
    CON_WT *start_ctype, *cwp;
    VEC cent_vec;
    VEC_PAIR vpair;

    PRINT_TRACE (2, print_string, "Trace: entering q_tr_cl_sa");
    PRINT_TRACE (6, print_tr_vec, in_tr);

    qsort((char *) in_tr->tr, in_tr->num_tr, sizeof(TR_TUP), comp_rank);

    /* Get vectors for top-ranked documents */
    for (i = 0; i < top_reranked; i++) {
	docid.id = in_tr->tr[i].did; EXT_NONE(docid.ext);
	if (UNDEF == did_vec(&docid, &veclist[i], did_vec_inst) ||
	    UNDEF == save_vec(&veclist[i]))
	    return(UNDEF);
    }

    /* Compute pairwise similarities */
    num_results = 0;
    for (i = 0; i < top_reranked; i++) {
	for (j = i+1; j < top_reranked; j++) {
	    simlist[num_results].qid.id = i; 
	    EXT_NONE(simlist[num_results].qid.ext);
	    vpair.vec1 = &veclist[i];
	    simlist[num_results].did.id = j;
	    EXT_NONE(simlist[num_results].did.ext);
	    vpair.vec2 = &veclist[j];
	    simlist[num_results].sim = 0.0001;
	    if (UNDEF == vec_vec_ptab->proc(&vpair, &simlist[num_results].sim,
					    vv_inst))
		return(UNDEF);
	    num_results++;
	}
    }

    if (UNDEF == generate_clusters())
	return(UNDEF);

    /* Make all similarities negative */
    for (i = 0; i < top_reranked; i++) 
	in_tr->tr[i].sim *= -1;
    /* Give positive similarities to clustered documents */
    for (i = 0; i < top_reranked; i++)
	if (cluster[i].num > 1)
	    for (j = 0; j < cluster[i].num; j++)
		for (k = 0; k < top_reranked; k++)
		    if (in_tr->tr[k].did == cluster[i].eid[j].id) {
			in_tr->tr[k].sim *= -1;
			/* HACK: save relevance information for this doc
			 * in the ext field of the eid. */
			cluster[i].eid[j].ext.num = in_tr->tr[k].rel;
			break;
		    }

    /* Print cluster information */
    if (cent_flag) {
#ifdef DEBUG
	printf(SEPARATOR); printf("%ld", in_tr->qid); printf(SEPARATOR);
#endif
	for (i = j = 0; i < top_reranked; i++) {
	    if (cluster[i].num > 1) {
		/* Compute the final centroid vector that will be 
		 * written out. This is potentially different from
		 * the centroid vectors used for the generation of
		 * the clusters.
		 * Usually, create_cent is used simply to add up all the
		 * individual vectors (i.e. save.centroid.normalize.tri_
		 * weight = nnn), and the averaging is explicitly done
		 * here.
		 */
		elist.num_eids = cluster[i].num; 
		elist.eid = cluster[i].eid;
		if (UNDEF == create_cent(&elist, &cent_vec, save_cent_inst))
		    return(UNDEF);
		for (k = 0; k < cent_vec.num_conwt; k++)
		    cent_vec.con_wtp[k].wt /= cluster[i].num;

		/* Truncate */
		for (k=0, start_ctype = cwp = cent_vec.con_wtp,
			 cent_vec.num_conwt = 0;
		     k < cent_vec.num_ctype; 
		     k++) {
		    ctype_num_terms = cent_vec.ctype_len[k];
		    qsort((char *)cwp, ctype_num_terms, sizeof(CON_WT), 
			  comp_wt);
		    ctype_num_terms = MIN(ctype_num_terms, num_terms[k]);
		    qsort((char *)cwp, ctype_num_terms, sizeof(CON_WT), 
			  comp_con);
		    if (start_ctype != cwp) 
			bcopy(cwp, start_ctype,
			      ctype_num_terms * sizeof(CON_WT));
		    cwp += cent_vec.ctype_len[k];
		    start_ctype += ctype_num_terms;
		    cent_vec.ctype_len[k] = ctype_num_terms;
		    cent_vec.num_conwt += ctype_num_terms;
		}

		/* Print info */
#ifdef DEBUG
		printf("\nCluster %ld (%ld):", j+1, cluster[i].num);
		for (k = 0; k < cluster[i].num; k++) {
		    if (cluster[i].eid[k].ext.num) rel = "**";
		    else rel = "";
		    printf(" %s%ld%s", rel, cluster[i].eid[k].id, rel);
		}
		printf("\n");
		cent_vec.id_num.id = in_tr->qid;
		if (UNDEF == print_vec_dict(&cent_vec, NULL, print_inst))
		    return(UNDEF);
#endif
		/* Save to file */
		cent_vec.id_num.id = in_tr->qid * 100 + j++ + 1;
		cent_vec.id_num.ext.type = 0;
		/* HACK: save the number of docs in this cluster in
		 * the ext.num field. */
		cent_vec.id_num.ext.num = cluster[i].num;
		if (UNDEF == seek_vector(cent_fd, &cent_vec) ||
		    UNDEF == write_vector(cent_fd, &cent_vec))
		    return(UNDEF);
	    }
	}
    }
    for (i=0; i < top_reranked; i++)
	free_vec(&veclist[i]);

    /* Compute new ranks and write the result out */
    /* Enough to sort only top_reranked, since actual similarities can
     * only have increased. */
    qsort((char *) in_tr->tr, top_reranked, sizeof(TR_TUP), comp_sim);
    /* keep only clustered docs (i.e. docs with +ve sims) */
    for (i = 0; i < top_reranked; i++) {
	if (in_tr->tr[i].sim > 0.0)
	    in_tr->tr[i].rank = i+1;
	else break;
    }
    in_tr->num_tr = i;
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

    PRINT_TRACE (4, print_top_results, out_res);
    PRINT_TRACE (2, print_string, "Trace: leaving q_tr_cl_sa");
    return (1);
}

int
close_q_tr_cl_sa (inst)
int inst;
{
    int i;

    PRINT_TRACE (2, print_string, "Trace: entering close_q_tr_cl_sa");

    if (UNDEF == close_did_vec(did_vec_inst) ||
	UNDEF == vec_vec_ptab->close_proc(vv_inst) ||
	UNDEF == close_create_cent(gen_cent_inst) ||
	UNDEF == close_create_cent(save_cent_inst) ||
	UNDEF == close_print_vec_dict(print_inst))
	return(UNDEF);

    Free(veclist); 
    Free(simlist);
    for (i=0; i < top_reranked; i++) 
	Free(cluster[i].eid);
    Free(cluster);
    Free(tr_buf);
    if (cent_flag) {
	Free(num_terms);
	if (UNDEF == close_vector (cent_fd))
	    return(UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_q_tr_cl_sa");
    return (0);
}


static int
generate_clusters()
{
    int num_pairs, max_index, c1, c2, tmp, i;
    float max_sim;
    EID_LIST elist;
    VEC_PAIR vpair;

    num_pairs = (top_reranked * (top_reranked - 1))/2;

    /* Initialization: one doc per cluster */
    for (i = 0; i < top_reranked; i++) {
	cluster[i].num = 1;
	cluster[i].eid[0] = veclist[i].id_num;
	cluster[i].vec = &veclist[i];
    }

    do {
	/* Find the nearest clusters */
	max_sim = 0.0; max_index = 0;
	for (i = 0; i < num_pairs; i++)
	    if (simlist[i].sim > max_sim) {
		max_sim = simlist[i].sim;
		max_index = i;
	    }
	c1 = simlist[max_index].qid.id;
	c2 = simlist[max_index].did.id;
	if (cluster[c1].num < cluster[c2].num)
	    tmp = c1, c1 = c2, c2 = tmp;
	/* Merge clusters if possible */
	if (max_sim > cluster_sim) {
#ifdef DEBUG
	    printf("Merging %d into %d (sim %6.2f)\n", c2, c1, simlist[max_index].sim);
#endif
	    for (i = cluster[c1].num; i < cluster[c1].num+cluster[c2].num; i++)
		cluster[c1].eid[i] = cluster[c2].eid[i-cluster[c1].num];
	    cluster[c1].num += cluster[c2].num;
	    elist.num_eids = cluster[c1].num; elist.eid = cluster[c1].eid;
	    free_vec(cluster[c1].vec);
	    if (UNDEF == create_cent(&elist, cluster[c1].vec, gen_cent_inst))
		return(UNDEF);
	    if (UNDEF == save_vec(cluster[c1].vec))
		return(UNDEF);
	    cluster[c2].num = 0;
	    /* Recompute similarities */
	    for (i = 0; i < num_pairs; i++) {
		if (simlist[i].sim == 0.0) /* eliminated by earlier pass */
		    continue;
		if (simlist[i].qid.id == c2 || simlist[i].did.id == c2) 
		    simlist[i].sim = 0.0;
		else if (simlist[i].qid.id == c1 || simlist[i].did.id == c1) {
		    vpair.vec1 = cluster[simlist[i].qid.id].vec; 
		    vpair.vec2 = cluster[simlist[i].did.id].vec; 
		    if (UNDEF == vec_vec_ptab->proc(&vpair, &simlist[i].sim,
						    vv_inst))
			return(UNDEF);
		}
	    }
	}
    } while (max_sim > cluster_sim);

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
