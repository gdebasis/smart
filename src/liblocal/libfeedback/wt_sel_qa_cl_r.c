#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libfeedback/wt_fdbk_sel_q.c,v 11.2 1993/02/03 16:50:03 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "feedback.h"
#include "io.h"
#include "proc.h"

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Low-level rocchio feedback weighting procedure
 *1 local.feedback.weight.fdbk_sel_q_all
 *2 wtfdbk_sel_q_all_clstrretro (unused1, fdbk_info, inst)
 *3   char *unused1;
 *3   FEEDBACK_INFO *fdbk_info;
 *3   int inst;
 *4 init_wtfdbk_sel_q_all_clstrretro (spec, unused)
 *5   "feedback.alpha"
 *5   "feedback.beta"
 *5   "feedback.gamma"
 *5   "feedback.weight.trace"
 *4 close_wtfdbk_sel_q_all_clstrretro (inst)
 *7 Cluster based expansion. bnn.btn weights used.
***********************************************************************/

static float alpha;
static float beta;
static float gammavar;
static char *textloc_file, *cluster_file;
static long max_clusters;
static float rel_density;
static char *apportion;

static SPEC_PARAM spec_args[] = {
    {"feedback.alpha",	getspec_float,	(char *) &alpha},
    {"feedback.beta",	getspec_float,	(char *) &beta},
    {"feedback.gamma",	getspec_float,	(char *) &gammavar},
    {"feedback.doc.textloc_file", getspec_dbfile, (char *) &textloc_file},
    {"feedback.cluster_file",	getspec_dbfile,	(char *)&cluster_file},
    {"feedback.cluster.top_wanted",	getspec_long,	(char *)&max_clusters},
    {"feedback.rel_density",	getspec_float,	(char *)&rel_density},
    {"feedback.apportion_by",	getspec_string,	(char *)&apportion},
    TRACE_PARAM ("feedback.weight.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static char *prefix;
static long num_expand; /* Number of terms to add */
static SPEC_PARAM_PREFIX prefix_spec_args[] = {
    {&prefix, "num_expand",       getspec_long, (char *) &num_expand}
};
static int num_prefix_spec_args =
    sizeof (prefix_spec_args) / sizeof (prefix_spec_args[0]);

typedef struct {
    VEC vec;
    float sim;
} CLSTR_VEC_SIM;

static SPEC *saved_spec;
static int cluster_fd;
static long num_docs = 0;
static CLSTR_VEC_SIM *clusters;

static int in_cluster(), compare_con(), compare_q_wt();

int
init_wtfdbk_sel_q_all_clstrretro (spec, unused)
SPEC *spec;
char *unused;
{
    REL_HEADER *rh;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_wtfdbk_sel_q_all_clstrretro");

    if (NULL != (rh = get_rel_header (textloc_file)) && rh->num_entries > 0)
	num_docs = rh->num_entries;
    else {
	set_error (SM_INCON_ERR, textloc_file, "init_wtfdbk_sel_q_all_clstrretro");
	return(UNDEF);
    }
    if (NULL == (clusters = Malloc(max_clusters, CLSTR_VEC_SIM)))
	return(UNDEF);
    if (UNDEF == (cluster_fd = open_vector(cluster_file, SRDONLY)))
	return(UNDEF);

    saved_spec = spec;

    PRINT_TRACE (2, print_string, "Trace: leaving init_wtfdbk_sel_q_all_clstrretro");
    return (1);
}

int
wtfdbk_sel_q_all_clstrretro (unused, fdbk_info, inst)
char *unused;
FEEDBACK_INFO *fdbk_info;
int inst;
{
    char param_prefix[PATH_LEN];
    long num_clusters, num_qterms, num_terms_sel, cluster_size, num_rel;
    long i, j, k;
    double rel_wt, nrel_wt, sum_old, sum_new, sim_sum;
    OCC *occp, *start_ctype, *end_occ;
    VEC ctype_vec;

    PRINT_TRACE (2, print_string, "Trace: entering wtfdbk_sel_q_all_clstrretro");

    for (i = 0; i < fdbk_info->num_occ; i++) {
        if (fdbk_info->num_rel > 0) 
            rel_wt = beta * fdbk_info->occ[i].rel_weight /
                fdbk_info->num_rel;
        else
            rel_wt = 0.0;

	nrel_wt = gammavar * fdbk_info->occ[i].nrel_weight /
	    (num_docs - fdbk_info->num_rel);

        fdbk_info->occ[i].weight = alpha * fdbk_info->occ[i].orig_weight + 
                                   rel_wt - nrel_wt;
        if (fdbk_info->occ[i].weight < 0.0)
            fdbk_info->occ[i].weight = 0.0;
    }

    /* Get each cluster centroid vector */
    for (num_clusters = 0; num_clusters < max_clusters; num_clusters++) { 
	clusters[num_clusters].vec.id_num.id = num_clusters + 1 + 
	    fdbk_info->orig_query->id_num.id*100;
	if (1 != seek_vector(cluster_fd, &clusters[num_clusters].vec) ||
	    1 != read_vector(cluster_fd, &clusters[num_clusters].vec))
	    break;	/* done with centroids for this query */
	if (UNDEF == save_vec(&clusters[num_clusters].vec))
	    return(UNDEF);
#if 1
	printf("Query %ld\tCluster %ld\t%d/%d\n",
	       fdbk_info->orig_query->id_num.id, num_clusters+1, 
	       clusters[num_clusters].vec.id_num.ext.num % 100,
	       clusters[num_clusters].vec.id_num.ext.num / 100);
#endif
    }

    /* Pick the clusters that have a higher rel_density than the threshold */
    for (i = 0, sim_sum = 0.0; i < num_clusters; i++) {
	/* HACK: tr_tr_rerank.cluster*.c has stored the no of docs
	 * and no. of rel docs in a cluster in the vec.id_num.ext.num 
	 * field.
	 */
	cluster_size = clusters[i].vec.id_num.ext.num / 100;
	num_rel = clusters[i].vec.id_num.ext.num % 100;
	if (100.0 * (float)num_rel / (float) cluster_size >= rel_density) {
	    /* select this cluster */
	    float sim = 0.0;
	    if (strcmp(apportion, "dens") == 0)
		sim = (float)num_rel / (float) cluster_size;
	    if (strcmp(apportion, "numrel") == 0)
		sim = num_rel;
	    if (strcmp(apportion, "numdoc") == 0)
		sim = cluster_size;
	    clusters[i].sim = sim;
	    sim_sum += sim;
	}
	else clusters[i].sim = 0.0;
    }
    for (i = 0; i < num_clusters; i++) clusters[i].sim /= sim_sum;

    /* keep all query terms and num_expand terms for each ctype */
    /* fdbk_info is initially sorted by <ctype, con> */
    start_ctype = occp = fdbk_info->occ;
    end_occ = fdbk_info->occ + fdbk_info->num_occ;
    for (i = 0; i <= (end_occ - 1)->ctype; i++) {
	(void) sprintf (param_prefix, "local.feedback.ctype.%ld.", i);
	prefix = param_prefix;
	if (UNDEF == lookup_spec_prefix(saved_spec,
					&prefix_spec_args[0],
					num_prefix_spec_args))
	    return (UNDEF);
	while (occp < end_occ && occp->ctype == i) 
	    occp++;

	/* sort terms by query followed by final weight */
	qsort((char *) start_ctype, occp - start_ctype, sizeof(OCC), 
	      compare_q_wt);
	for (j = 0; j < occp - start_ctype && start_ctype[j].query; j++);
	num_qterms = j;

	/* Get terms from each cluster */
	/* HACK: use the rel_weight field to denote whether a term
	 * has been selected (from some centroid) for inclusion in
	 * the final query. */
	for (; j < occp - start_ctype; j++)
	    start_ctype[j].rel_weight = 0.0;
	for (j = 0; j < num_clusters; j++) {
	    ctype_vec.con_wtp = clusters[j].vec.con_wtp;
	    for (k = 0; k < start_ctype->ctype; k++)
		ctype_vec.con_wtp += clusters[j].vec.ctype_len[k];
            ctype_vec.num_conwt = clusters[j].vec.ctype_len[k];

	    for (k = num_qterms, num_terms_sel = 0; 
		 k < occp - start_ctype && 
		     num_terms_sel < (long) floor(clusters[j].sim*num_expand + 
						  0.5); 
		 k++) {
		if (in_cluster(start_ctype[k].con, &ctype_vec)) {
		    start_ctype[k].rel_weight += 1.0;
		    num_terms_sel++;
		}
	    }
#if 1
printf("Selected %ld terms from cluster %ld\n", 
       num_terms_sel, clusters[j].vec.id_num.id);
#endif
	}
	/* if a term was not selected by any cluster zero it out */
	for (j = num_qterms; j < occp - start_ctype; j++)
	    if (start_ctype[j].rel_weight == 0.0)
		start_ctype[j].weight = 0.0;
	/* sort back by con */
	qsort((char *) start_ctype, occp - start_ctype, sizeof(OCC), 
	      compare_con);
	start_ctype = occp;
    }

    /* scale final wts to ensure compatibility with original query wts */
    sum_old = 0;
    for (i = 0; i < fdbk_info->orig_query->num_conwt; i++)
	sum_old += fdbk_info->orig_query->con_wtp[i].wt;
    assert(sum_old > 0);
    sum_new = 0;
    for (i = 0; i < fdbk_info->num_occ; i++) 
	sum_new += fdbk_info->occ[i].weight;
    for (i = 0; i < fdbk_info->num_occ; i++) 
	fdbk_info->occ[i].weight *= sum_old/sum_new;

    for (i=0; i < num_clusters; i++)
	free_vec(&clusters[i].vec);

    PRINT_TRACE (4, print_fdbk_info, fdbk_info);
    PRINT_TRACE (2, print_string, "Trace: leaving wtfdbk_sel_q_all_clstrretro");

    return (1);
}

int
close_wtfdbk_sel_q_all_clstrretro (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_wtfdbk_sel_q_all_clstrretro");
    Free(clusters);
    if (UNDEF == close_vector(cluster_fd))
	return(UNDEF);
    PRINT_TRACE (2, print_string, "Trace: entering close_wtfdbk_sel_q_all_clstrretro");
    return (0);
}

static int
in_cluster(con, vec)
long con;
VEC *vec;
{
    int i;

    for (i=0; i < vec->num_conwt; i++) 
        if (vec->con_wtp[i].con == con)
            return 1;
    return 0;
}

static int
compare_con (ptr1, ptr2)
OCC *ptr1;
OCC *ptr2;
{
    if (ptr1->con < ptr2->con)
        return (-1);
    if (ptr1->con > ptr2->con)
        return (1);
    return (0);
}

/* Terms sorted by final weight */
static int
compare_q_wt (ptr1, ptr2)
OCC *ptr1;
OCC *ptr2;
{
    if (ptr1->query) {
        if (ptr2->query)
            return (0);
        return (-1);
    }
    if (ptr2->query)
        return (1);

    if (ptr1->weight > ptr2->weight)
        return (-1);
    if (ptr1->weight < ptr2->weight)
        return (1);
    return (0);
}
