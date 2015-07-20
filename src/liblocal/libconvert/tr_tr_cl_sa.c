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
 *1 local.convert.rerank.cluster
 *2 rerank_cluster (in_tr_file, out_tr_file, inst)
 *3 char *in_tr_file;
 *3 char *out_tr_file;
 *3 int inst;
 *4 init_rerank_cluster (spec, unused)
 *5   "rerank.in_tr_file"
 *5   "rerank.in_tr_file.rmode"
 *5   "rerank.out_tr_file"
 *5   "rerank.trace"
 *4 close_rerank_cluster (inst)

 *7 This should go somewhere in libretrieve.
 *7 Cluster the top ranked documents.
 *7 Similar to tr_tr_rerank.cluster-nosel.c.
 *7 Only the documents that belong to clusters are included in the out tr
 *7 file. Thus, the out tr file is much shorter compared to the in tr file.
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

static char *default_tr_file;
static long tr_mode;
static char *out_tr_file;
static long top_wanted;
static float factor;
static PROC_TAB *vec_vec_ptab;
static float cluster_sim;
static long num_ctypes;
static long cent_flag;
static char *cent_file;

static SPEC_PARAM spec_args[] = {
    {"rerank.in_tr_file",	getspec_dbfile,	(char *) &default_tr_file},
    {"rerank.in_tr_file.rmode",	getspec_filemode,	(char *) &tr_mode},
    {"rerank.out_tr_file",	getspec_dbfile,	(char *) &out_tr_file},
    {"rerank.top_wanted",	getspec_long,	(char *) &top_wanted},    
    {"rerank.factor",		getspec_float,	(char *) &factor},    
    {"rerank.vec_vec",		getspec_func,	(char *)&vec_vec_ptab},
    {"rerank.cluster_sim",	getspec_float,	(char *) &cluster_sim},
    {"rerank.num_ctypes",	getspec_long,	(char *) &num_ctypes},
    {"rerank.cent_flag",	getspec_bool,	(char *) &cent_flag},
    {"rerank.cent_file",	getspec_dbfile,	(char *) &cent_file},
    TRACE_PARAM ("rerank.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static char *prefix;
static long num;
static SPEC_PARAM_PREFIX pspec_args[] = {
    {&prefix, "cent_num_terms",	getspec_long,	(char *) &num},
    };
static int num_pspec_args = sizeof (pspec_args) / sizeof (pspec_args[0]);

int vec_inst, vv_inst, gen_cent_inst, save_cent_inst, print_inst;  
long *num_terms;
VEC *veclist;
RESULT_TUPLE *simlist;
CLUSTER *cluster;

static int generate_clusters();
static int comp_rank(), comp_sim(), comp_did(), comp_con(), comp_wt();


int
init_rerank_cluster_selall (spec, unused)
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

    PRINT_TRACE (2, print_string, "Trace: entering init_rerank_cluster_selall");

    if (UNDEF == (vec_inst = init_did_vec(spec, NULL)) ||
	UNDEF == (vv_inst = vec_vec_ptab->init_proc(spec, NULL)) ||
	UNDEF == (gen_cent_inst = init_create_cent(spec, "gen.cluster.")) ||
	UNDEF == (save_cent_inst = init_create_cent(spec, "save.cluster.")) ||
	UNDEF == (print_inst = init_print_vec_dict(spec, NULL)))
	return(UNDEF);

    if (top_wanted <= 0) {
	set_error(SM_ILLPA_ERR, "top_wanted", "init_rerank_cluster_selall");
	return(UNDEF);
    }
    if (NULL == (veclist = Malloc(top_wanted, VEC)) ||
	NULL == (simlist = Malloc((top_wanted*(top_wanted-1))/2, RESULT_TUPLE)) ||
	NULL == (cluster = Malloc(top_wanted, CLUSTER)))
	return(UNDEF);

    for (i = 0; i < top_wanted; i++)
	if (NULL == (cluster[i].eid = Malloc(top_wanted, EID)))
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

    PRINT_TRACE (2, print_string, "Trace: leaving init_rerank_cluster_selall");

    return (0);
}

#define SEPARATOR "\n-------------------------------------------------------\n"

int
rerank_cluster_selall (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
    char *rel;
    int in_fd, out_fd, vec_fd;
    long num_results, ctype_num_terms, i, j, k;
    EID docid;
    EID_LIST elist;
    CON_WT *start_ctype, *cwp;
    VEC cent_vec;
    VEC_PAIR vpair;
    TR_VEC tr_vec;

    PRINT_TRACE (2, print_string, "Trace: entering rerank_cluster_selall");

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
    if (cent_flag &&
	UNDEF == (vec_fd = open_vector (cent_file, SRDWR))) {
	clr_err();
	if (UNDEF == (vec_fd = open_vector (cent_file, SRDWR|SCREATE)))
	    return(UNDEF);
    }

    while (1 == read_tr_vec (in_fd, &tr_vec)) {
	if(tr_vec.qid < global_start) continue;
	if(tr_vec.qid > global_end) break;
	qsort((char *) tr_vec.tr, tr_vec.num_tr, sizeof(TR_TUP), comp_rank);

	/* Get vectors for top-ranked documents */
        for (i = 0; i < top_wanted; i++) {
	    docid.id = tr_vec.tr[i].did; EXT_NONE(docid.ext);
	    if (UNDEF == did_vec(&docid, &veclist[i], vec_inst) ||
		UNDEF == save_vec(&veclist[i]))
		return(UNDEF);
	}

	/* Compute pairwise similarities */
	num_results = 0;
	for (i = 0; i < top_wanted; i++) {
	    for (j = i+1; j < top_wanted; j++) {
		simlist[num_results].qid.id = i; 
		EXT_NONE(simlist[num_results].qid.ext);
		vpair.vec1 = &veclist[i];
		simlist[num_results].did.id = j;
		EXT_NONE(simlist[num_results].did.ext);
		vpair.vec2 = &veclist[j];
		simlist[num_results].sim = 0.0001;
		if (UNDEF == vec_vec_ptab->proc(&vpair,
						&simlist[num_results].sim,
						vv_inst))
		    return(UNDEF);
		num_results++;
	    }
	}

	if (UNDEF == generate_clusters())
	    return(UNDEF);

	/* Make all similarities negative */
	for (i = 0; i < top_wanted; i++) 
	    tr_vec.tr[i].sim *= -1;
	/* Give positive similarities to clustered documents */
	for (i = 0; i < top_wanted; i++)
	    if (cluster[i].num > 1)
		for (j = 0; j < cluster[i].num; j++)
		    for (k = 0; k < top_wanted; k++)
			if (tr_vec.tr[k].did == cluster[i].eid[j].id) {
			    tr_vec.tr[k].sim *= -1;
			    /* HACK: save relevance information for this doc
			     * in the ext field of the eid. */
			    cluster[i].eid[j].ext.num = tr_vec.tr[k].rel;
			    break;
			}

	/* Print cluster information */
	if (cent_flag) {
#ifdef DEBUG
	    printf(SEPARATOR); printf("%ld", tr_vec.qid); printf(SEPARATOR);
#endif
	    for (i = j = 0; i < top_wanted; i++) {
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
		    if (UNDEF==create_cent(&elist, &cent_vec, save_cent_inst))
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
				  ctype_num_terms*sizeof(CON_WT));
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
		    cent_vec.id_num.id = tr_vec.qid;
		    if (UNDEF == print_vec_dict(&cent_vec, NULL, print_inst))
			return(UNDEF);
#endif
		    /* Save to file */
		    cent_vec.id_num.id = tr_vec.qid * 100 + j++ + 1;
		    cent_vec.id_num.ext.type = 0;
		    /* HACK: save the number of docs in this cluster in
		     * the ext.num field. */
		    cent_vec.id_num.ext.num = cluster[i].num;
		    if (UNDEF == seek_vector(vec_fd, &cent_vec) ||
			UNDEF == write_vector(vec_fd, &cent_vec))
			return(UNDEF);
		}
	    }
	}
	for (i=0; i < top_wanted; i++)
	    free_vec(&veclist[i]);

	/* Compute new ranks and write the result out */
	/* Enough to sort only top_wanted, since actual similarities can
	 * only have increased. */
	qsort((char *) tr_vec.tr, top_wanted, sizeof(TR_TUP), comp_sim);
	/* keep only clustered docs (i.e. docs with +ve sims) */
	for (i = 0; i < top_wanted; i++) {
	    if (tr_vec.tr[i].sim > 0.0)
		tr_vec.tr[i].rank = i+1;
	    else break;
	}
	tr_vec.num_tr = i;
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

    PRINT_TRACE (2, print_string, "Trace: leaving rerank_cluster_selall");
    return (1);
}

int
close_rerank_cluster_selall (inst)
int inst;
{
    int i;

    PRINT_TRACE (2, print_string, "Trace: entering close_rerank_cluster_selall");

    if (UNDEF == close_did_vec(vec_inst) ||
	UNDEF == vec_vec_ptab->close_proc(vv_inst) ||
	UNDEF == close_create_cent(gen_cent_inst) ||
	UNDEF == close_create_cent(save_cent_inst) ||
	UNDEF == close_print_vec_dict(print_inst))
	return(UNDEF);

    Free(veclist); 
    Free(simlist);
    for (i=0; i < top_wanted; i++) 
	Free(cluster[i].eid);
    Free(cluster);

    PRINT_TRACE (2, print_string, "Trace: leaving close_rerank_cluster_selall");
    return (0);
}


static int
generate_clusters()
{
    int num_pairs, max_index, c1, c2, tmp, i;
    float max_sim;
    EID_LIST elist;
    VEC_PAIR vpair;

    num_pairs = (top_wanted * (top_wanted - 1))/2;

    /* Initialization: one doc per cluster */
    for (i = 0; i < top_wanted; i++) {
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
