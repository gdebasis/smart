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
 *2 wtfdbk_sel_q_all_clstrpidf (unused1, fdbk_info, inst)
 *3   char *unused1;
 *3   FEEDBACK_INFO *fdbk_info;
 *3   int inst;
 *4 init_wtfdbk_sel_q_all_clstrpidf (spec, unused)
 *5   "feedback.alpha"
 *5   "feedback.beta"
 *5   "feedback.gamma"
 *5   "feedback.weight.trace"
 *4 close_wtfdbk_sel_q_all_clstrpidf (inst)
 *7 Same as feedback.weight.fdbk_all (libfeedback/wt_fdbk_all.c), except 
 *7 that selection of ctype dependent num_expand terms occurs here. 
 *7 Term selection based on final Rochhio weight.
 *7 Query terms always included.
***********************************************************************/

#define MAX_CORR 2048

typedef struct correlation {
    long qid;
    long con1, con2;
    long common, count1, count2;
} CORRELATION;

typedef struct match {
    long con;
    float qwt, dwt;
    long num_docs;
} MATCH;

static float alpha;
static float beta;
static float gammavar;
static char *textloc_file, *cluster_file, *corr_file;
static long max_clusters, num_top_docs, min_clusters, overlap_flag;

static SPEC_PARAM spec_args[] = {
    {"feedback.alpha",	getspec_float,	(char *) &alpha},
    {"feedback.beta",	getspec_float,	(char *) &beta},
    {"feedback.gamma",	getspec_float,	(char *) &gammavar},
    {"feedback.doc.textloc_file", getspec_dbfile, (char *) &textloc_file},
    {"feedback.cluster_file",	getspec_dbfile,	(char *)&cluster_file},
    {"feedback.cluster.top_wanted",	getspec_long,	(char *)&max_clusters},
    {"feedback.num_assume_rel",	getspec_long,	(char *)&num_top_docs},
    {"feedback.min_clusters",	getspec_long,	(char *)&min_clusters},
    {"rerank.corr_file",	getspec_dbfile,	(char *) &corr_file},
    {"feedback.cluster.overlap_wt",	getspec_bool,	(char *)&overlap_flag},
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
static int qvec_inst, cluster_fd;
static long num_docs = 0;
static CLSTR_VEC_SIM *clusters;
static long max_corr, num_corr;
static CORRELATION *corr;

static int get_corr(), num_matches(), in_cluster();
static int compare_con(), compare_q_wt(), compare_match(), comp_cluster();

int
init_wtfdbk_sel_q_all_clstrpidf (spec, unused)
SPEC *spec;
char *unused;
{
    FILE *fp;
    REL_HEADER *rh;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_wtfdbk_sel_q_all_clstrpidf");

    if (NULL != (rh = get_rel_header (textloc_file)) && rh->num_entries > 0)
	num_docs = rh->num_entries;
    else {
	set_error (SM_INCON_ERR, textloc_file, "init_wtfdbk_sel_q_all_clstrpidf");
	return(UNDEF);
    }
    if (UNDEF == (qvec_inst = init_qid_vec (spec, (char *) NULL)))
	return(UNDEF);
    if (NULL == (clusters = Malloc(max_clusters, CLSTR_VEC_SIM)))
	return(UNDEF);
    if (UNDEF == (cluster_fd = open_vector(cluster_file, SRDONLY)))
	return(UNDEF);


    max_corr = MAX_CORR;
    if (NULL == (corr = Malloc(max_corr, CORRELATION)) ||
	NULL == (fp = fopen(corr_file, "r")))
	return(UNDEF);
    get_corr(fp);
    fclose(fp);

    saved_spec = spec;

    PRINT_TRACE (2, print_string, "Trace: leaving init_wtfdbk_sel_q_all_clstrpidf");
    return (1);
}

int
wtfdbk_sel_q_all_clstrpidf (unused, fdbk_info, inst)
char *unused;
FEEDBACK_INFO *fdbk_info;
int inst;
{
    char param_prefix[PATH_LEN];
    long num_clusters, num_qterms, num_terms_sel, num_sel_clstr, docs_count;
    long i, j, k;
    double rel_wt, nrel_wt, sum_old, sum_new, sim_sum;
    OCC *occp, *start_ctype, *end_occ;
    VEC qvec, ctype_vec;
    VEC_PAIR vpair;
    CORRELATION *this_query;

    PRINT_TRACE (2, print_string, "Trace: entering wtfdbk_sel_q_all_clstrpidf");

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

    /* Get weighted query vector and each cluster centroid vector */
    qvec.id_num = fdbk_info->orig_query->id_num;
    if (UNDEF == qid_vec(&(qvec.id_num.id), &qvec, qvec_inst))
	return(UNDEF);
    for (this_query = corr; 
	 this_query < corr + num_corr && this_query->qid != qvec.id_num.id; 
	 this_query++);

    for (num_clusters = 0; num_clusters < max_clusters; num_clusters++) { 
	clusters[num_clusters].vec.id_num.id = num_clusters + 1 + 
	    fdbk_info->orig_query->id_num.id*100;
	if (1 != seek_vector(cluster_fd, &clusters[num_clusters].vec) ||
	    1 != read_vector(cluster_fd, &clusters[num_clusters].vec))
	    break;	/* done with centroids for this query */
	if (UNDEF == save_vec(&clusters[num_clusters].vec))
	    return(UNDEF);
    }

    /* Compute cluster-query similarities */
    for (i = 0; i < num_clusters; i++) {
	vpair.vec1 = &qvec;
	vpair.vec2 = &(clusters[i].vec);
	if (UNDEF == num_matches(&vpair, this_query, &(clusters[i].sim)))
	    return(UNDEF);

#if 1
printf("Query %ld\tCluster %ld\tSim %8.3f\n", fdbk_info->orig_query->id_num.id,
       i+1, clusters[i].sim); 
#endif

    }
    qsort((char *)clusters, num_clusters, sizeof(CLSTR_VEC_SIM), comp_cluster);
    /* Pick the best clusters until you have num_docs docs */
    for (i = 0, docs_count = 0, sim_sum = 0.0; 
	 i < num_clusters && (i < min_clusters || docs_count < num_top_docs); 
	 i++) {
	/* HACK: tr_tr_rerank.cluster*.c has stored the number of docs
	 * in a cluster in the vec.id_num.ext.num field.
	 */
	docs_count += clusters[i].vec.id_num.ext.num;
	sim_sum += clusters[i].sim;
    }
    num_sel_clstr = i;
    for (i = 0; i < num_sel_clstr; i++)
	clusters[i].sim /= sim_sum;

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
	for (j = 0; j < num_sel_clstr; j++) {
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
	    else if (overlap_flag) 
		start_ctype[j].weight *= (1 + log(start_ctype[j].rel_weight));
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
    PRINT_TRACE (2, print_string, "Trace: leaving wtfdbk_sel_q_all_clstrpidf");

    return (1);
}

int
close_wtfdbk_sel_q_all_clstrpidf (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_wtfdbk_sel_q_all_clstrpidf");
    if (UNDEF == close_qid_vec(qvec_inst))
	return(UNDEF);
    Free(clusters);
    Free(corr);
    if (UNDEF == close_vector(cluster_fd))
	return(UNDEF);
    PRINT_TRACE (2, print_string, "Trace: entering close_wtfdbk_sel_q_all_clstrpidf");
    return (0);
}

static int
get_corr(fp)
FILE *fp;
{
    char inbuf[PATH_LEN];
    CORRELATION *cptr;

    num_corr = 0;
    cptr = corr;
    while (NULL != fgets(inbuf, PATH_LEN, fp)) {
	sscanf(inbuf, "%ld%ld%ld%ld%ld%ld\n",
	       &cptr->qid, &cptr->con1, &cptr->con2, 
	       &cptr->common, &cptr->count1, &cptr->count2);
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
num_matches(vpair, this_query, sim)
VEC_PAIR *vpair;
CORRELATION *this_query;
float *sim;
{
    long num_match, coni, conj, i, j, k;
    float prob, min_prob, total;
    VEC *query, *doc;
    MATCH *matchlist;

    query = vpair->vec1; doc = vpair->vec2;
    if (NULL == (matchlist = Malloc(query->ctype_len[0], MATCH)))
	return(UNDEF);

    /* Find the matching single terms for this window */
    for (i = 0, j = 0, k = 0, num_match = 0; 
	 i < doc->ctype_len[0] && j < query->ctype_len[0];) {
	if (doc->con_wtp[i].con < query->con_wtp[j].con)
	    i++;
	else if (doc->con_wtp[i].con > query->con_wtp[j].con)
	    j++;
	else {
	    matchlist[num_match].con = query->con_wtp[j].con;
	    matchlist[num_match].qwt = query->con_wtp[j].wt;
	    matchlist[num_match].dwt = doc->con_wtp[i].wt;
	    while (this_query[k].qid == query->id_num.id) {
		if (query->con_wtp[j].con == this_query[k].con1) {
		    matchlist[num_match].num_docs = this_query[k].count1;
		    break;
		}
		if (query->con_wtp[j].con == this_query[k].con2) {
		    matchlist[num_match].num_docs = this_query[k].count2;
		    break;
		}
		k++;
	    }
	    assert(this_query[k].qid == query->id_num.id);
	    i++; j++; num_match++;
	}
    }

    /* Sort matching terms in increasing order of num_docs */
    qsort((char *)matchlist, num_match, sizeof(MATCH), compare_match);

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

    *sim = total;

    Free(matchlist);
    return 1;
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

static int
compare_match(m1, m2)
MATCH *m1, *m2;
{
    if (m1->num_docs > m2->num_docs) return 1;
    if (m1->num_docs < m2->num_docs) return -1;

    if (m1->qwt > m2->qwt) return -1;
    if (m1->qwt < m2->qwt) return 1;

    return 0;
}

static int
comp_cluster(c1, c2)
CLSTR_VEC_SIM *c1;
CLSTR_VEC_SIM *c2;
{
    if (c1->sim > c2->sim)
	return -1;
    if (c1->sim < c2->sim)
	return 1;
    if (c1->vec.id_num.ext.num > c2->vec.id_num.ext.num)
	return 1;
    if (c1->vec.id_num.ext.num < c2->vec.id_num.ext.num)
	return -1;
    return 0;
}
