#ifdef RCSID
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Generate query term cluster ctypes for terms in major_ctype of query.
 *1 local.convert.obj.q_qtc
 *2 q_qtc (in_query, out_query, inst)
 *3   VEC * in_query;
 *3   VEC * out_query
 *3   int inst;

 *4 init_q_qtc (spec, unused)
 *5   "convert.q_qtc.orig_query_file"
 *5   "convert.q_qtc.major_ctype"
 *5   "convert.q_qtc.starting_cluster_ctype"
 *5   "convert.q_qtc.correlation_constant"
 *5   "convert.q_qtc.inv_file
 *5   "convert.q_qtc.inv_file.rmode
 *5   "convert.q_qtc.correlation_file"
 *5   "convert.q_qtc.correlation_file.rwmode"
 *5   "q_qtc.trace"
 *4 close_q_qtc(inst)

 *7 expands terms of major_ctype by adding SuperConcepts.
 *7 Return UNDEF if error, else 0;
 *7 Assumption: incoming vectors are sorted by con, ctype.
***********************************************************************/

#include "common.h"
#include "context.h"
#include "functions.h"
#include "inv.h"
#include "io.h"
#include "param.h"
#include "proc.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "vector.h"

static long max_allowed_clusters;
static long major_ctype;
static long starting_cluster_ctype;
static char *orig_query_file;
static long orig_query_mode;
static char *q_corr_file;           
static long q_corr_mode;
static float correlation_constant;
static char *textloc_file;
static char *doc_file;
static SPEC_PARAM spec_args[] = {
    {"convert.q_qtc.max_num_clusters", getspec_long, (char *) &max_allowed_clusters},
    {"convert.q_qtc.major_ctype", getspec_long, (char *) &major_ctype},
    {"convert.q_qtc.starting_cluster_ctype", getspec_long, (char *) &starting_cluster_ctype},
    {"convert.q_qtc.correlation_constant", getspec_float, (char *) &correlation_constant},
    {"convert.q_qtc.q_corr_file", getspec_dbfile, (char *) &q_corr_file},
    {"convert.q_qtc.q_corr_file.rwmode", getspec_filemode,(char *) &q_corr_mode},
    {"convert.q_qtc.orig_query_file",   getspec_dbfile, (char *) &orig_query_file},
    {"convert.q_qtc.orig_query_file.rmode", getspec_filemode,(char *) &orig_query_mode},
    {"doc.textloc_file",        getspec_dbfile, (char *) &textloc_file},
    {"doc.doc_file",            getspec_dbfile, (char *) &doc_file},

     TRACE_PARAM ("q_qtc.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


static char *prefix;
static char *inv_file;           
static long inv_mode;
static SPEC_PARAM_PREFIX pspec_args[] = {
    {&prefix, "inv_file", getspec_dbfile, (char *) &inv_file},
    {&prefix, "inv_file.rmode", getspec_filemode, (char *) &inv_mode},
};
static int num_pspec_args = sizeof (pspec_args) / sizeof (pspec_args[0]);

static int inv1_fd;
static int inv2_fd;
static int orig_query_fd;

typedef struct {
    long max_num_in_cluster;
    long num_in_cluster;
    float current_con_corr;
    CON_WT *con_wt;
} QTC;

static QTC *clusters;
static long num_clusters;
static long max_num_clusters = 0;

static long max_num_con_wt = 0;
static CON_WT *save_con_wt;
static long max_ctype_len = 0;
static long *save_ctype_len;

static long num_docs_in_collection;

static int create_new_query();
static void add_terms();
static int comp_dec_seed();
static float get_corr();
static void print_qtc();

int
init_q_qtc (spec, unused)
SPEC *spec;
char *unused;
{
    REL_HEADER *rh = NULL;
    char temp_prefix[50];

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_q_qtc");

    prefix = &temp_prefix[0];
    sprintf (prefix, "convert.q_qtc.ctype.%ld.", major_ctype);
    if (UNDEF == lookup_spec_prefix (spec, &pspec_args[0], num_pspec_args))
        return (UNDEF);
 

    /* Open two copies of the inverted file, so can keep pointers to two */
    /* inverted lists at once */
    if (UNDEF == (inv1_fd = open_inv(inv_file, inv_mode)))
        return (UNDEF);
    if (UNDEF == (inv2_fd = open_inv(inv_file, inv_mode)))
        return (UNDEF);

    /* Open original query file, to get seeds of clusters */
    if (UNDEF == (orig_query_fd = open_vector (orig_query_file, 
					       orig_query_mode)))
        return (UNDEF);

    /* Find number of docs in collection */
    if (VALID_FILE (textloc_file)) {
        if (NULL == (rh = get_rel_header (textloc_file))) {
            set_error (SM_ILLPA_ERR, textloc_file, "init_q_qtc");
            return (UNDEF);
        }
    }
    else if (NULL == (rh = get_rel_header (doc_file))) {
        set_error (SM_ILLPA_ERR, textloc_file, "init_q_qtc");
        return (UNDEF);
    }
    num_docs_in_collection = rh->num_entries + 1;

    /* Eventually open cache of previous correlations */

    PRINT_TRACE (2, print_string, "Trace: leaving init_q_qtc");

    return (0);
}

int
q_qtc (in_query, out_query, inst)
VEC *in_query;
VEC *out_query;
int inst;
{
    VEC orig_vector;
    long num_terms, new_num_clusters;
    long i, j, clust;
    long orig_query_flag;
    float summ_corr, corr;
    long con;
    long start_ctype_orig, start_ctype_in;
    CON_WT *orig_ptr, *in_ptr;

    PRINT_TRACE (2, print_string, "Trace: entering q_qtc");
    PRINT_TRACE (6, print_vector, in_query);

    orig_vector.id_num.id = in_query->id_num.id;
    if (1 != seek_vector (orig_query_fd, &orig_vector)||
	1 != read_vector (orig_query_fd, &orig_vector)) {
	return (UNDEF);
    }
    PRINT_TRACE (12, print_vector, &orig_vector);

    if (major_ctype >= in_query->num_ctype ||
	major_ctype >= orig_vector.num_ctype) {
        set_error (SM_INCON_ERR, "Major_ctype too large", "q_qtc");
        return (UNDEF);
    }

    new_num_clusters = orig_vector.ctype_len[major_ctype];
    num_terms = new_num_clusters + in_query->ctype_len[major_ctype];
    if (new_num_clusters > max_num_clusters) {
	if (max_num_clusters == 0) {
	    if (NULL == (clusters = (QTC *)
			 malloc ((unsigned) new_num_clusters * sizeof (QTC))))
		return (UNDEF);
	}
	else {
	    if (NULL == (clusters = (QTC *)
			 realloc ((char *) clusters,
				  (unsigned) new_num_clusters * sizeof (QTC))))
		return (UNDEF);
	}
	max_num_clusters = new_num_clusters;
    }

    /* Find starting location for major_ctype's con_wt */
    start_ctype_orig = 0;
    for (i = 0; i < major_ctype; i++)
	start_ctype_orig += orig_vector.ctype_len[i];
    start_ctype_in = 0;
    for (i = 0; i < major_ctype; i++)
	start_ctype_in += in_query->ctype_len[i];

    /* Make sure each cluster is big enough, and initialize it to an */
    /* original query term */
    for (i = 0; i < new_num_clusters; i++) {
	orig_ptr = &orig_vector.con_wtp[i+start_ctype_orig];
	if (i < num_clusters) {
	    if (clusters[i].max_num_in_cluster < num_terms) {
		(void) free ((char *) clusters[i].con_wt);
		if (NULL == (clusters[i].con_wt = (CON_WT *)
			     malloc ((unsigned) num_terms * sizeof (CON_WT))))
		    return (UNDEF);
		clusters[i].max_num_in_cluster = num_terms;
	    }
	}
	else {
	    if (NULL == (clusters[i].con_wt = (CON_WT *)
			 malloc ((unsigned) num_terms * sizeof (CON_WT))))
		return (UNDEF);
	    clusters[i].max_num_in_cluster = num_terms;
	}

	/* Make original query term the first term in cluster */
	clusters[i].con_wt[0].con = orig_ptr->con;
	clusters[i].con_wt[0].wt = 0;
	in_ptr = &in_query->con_wtp[start_ctype_in];
	for (j = 0; j < in_query->ctype_len[major_ctype]; j++) {
	    in_ptr++;
	    if (orig_ptr->con == in_ptr->con)
		break;
	}
	if (j < in_query->ctype_len[major_ctype])
	    clusters[i].con_wt[0].wt = in_ptr->wt;
	clusters[i].num_in_cluster = 1;
    }
    num_clusters = new_num_clusters;
    /* Sort by decreasing weight of cluster seed */
    (void) qsort ((char *) clusters,
		  num_clusters,
		  sizeof (QTC),
		  comp_dec_seed);
    if (num_clusters > max_allowed_clusters)
	num_clusters = max_allowed_clusters;

    PRINT_TRACE (8, print_qtc, clusters);
    
    /* for each query term, */
    /*       compute the correlation to each cluster seed */
    /*       distribute the weight of the term proportionally to each cluster*/
    for (i = 0; i < in_query->ctype_len[major_ctype]; i++) {
	in_ptr = &in_query->con_wtp[i+start_ctype_in];
	orig_query_flag = 0;
	summ_corr = 0.0;
	con = in_ptr->con;
	for (clust = 0; clust < num_clusters; clust++) {
	    if (con == clusters[clust].con_wt[0].con)
		/* Just mark as query term */
		orig_query_flag = 1;
	    else {
		clusters[clust].current_con_corr =
		                 get_corr (con, clusters[clust].con_wt[0].con);
		if (clusters[clust].current_con_corr <= 0.0)
		    clusters[clust].current_con_corr = 0.0;
		else 
		    summ_corr += clusters[clust].current_con_corr;
	    }
	}
	if (! orig_query_flag) {
	    for (clust = 0; clust < num_clusters; clust++) {
		if (clusters[clust].current_con_corr > 0.0) {
		    clusters[clust].con_wt[clusters[clust].num_in_cluster].con
			= con;
		    clusters[clust].con_wt[clusters[clust].num_in_cluster].wt
			= in_ptr->wt * clusters[clust].current_con_corr / summ_corr;
		    clusters[clust].num_in_cluster++;
		}
	    }
	}
	PRINT_TRACE (12, print_qtc, clusters);
    }  

    PRINT_TRACE (8, print_qtc, clusters);

    /* If we have correlated original query terms(seeds), now distribute */
    /* the weight of each cluster among its correlated seeds. */
    for (i = num_clusters - 1; i >= 0; i--) {
	for (clust = 0; clust < i; clust++) {
	    corr = get_corr (clusters[i].con_wt[0].con, clusters[clust].con_wt[0].con);
	    if (corr > 0.0) {
		add_terms (i, clust, &corr);
	    }
	}
    }

    PRINT_TRACE (8, print_qtc, clusters);

    /* Create new query from clusters and in_query */
    if (UNDEF == create_new_query (in_query, out_query))
	return (UNDEF);

    PRINT_TRACE (4, print_vector, out_query);
    PRINT_TRACE (2, print_string, "Trace: leaving q_qtc");
    return (1);
}

int
close_q_qtc(inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_q_qtc");

    if (UNDEF == close_inv(inv1_fd))
        return (UNDEF);
    if (UNDEF == close_inv(inv2_fd))
        return (UNDEF);

    if (UNDEF == close_vector(orig_query_fd))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering close_q_qtc");
    return (0);
}

/* Add the terms in cluster start to the cluster dest,assuming a correlation */
/* of corr between the two */
static void
add_terms (start, dest, corr)
long start, dest;
float *corr;
{
    CON_WT *ptr, *dest_ptr;
    CON_WT *end_ptr, *end_dest_ptr;
    float wt;

    PRINT_TRACE (12, print_long, &start);
    PRINT_TRACE (12, print_long, &dest);
    PRINT_TRACE (12, print_float, corr);
    end_ptr = &clusters[start].con_wt[clusters[start].num_in_cluster];
    end_dest_ptr = &clusters[dest].con_wt[clusters[dest].num_in_cluster];
    for (ptr = clusters[start].con_wt; ptr < end_ptr; ptr++) {
	wt = *corr * ptr->wt * correlation_constant;
	ptr->wt = ptr->wt - wt;
	dest_ptr = clusters[dest].con_wt + 1;
	while (dest_ptr < end_dest_ptr && dest_ptr->con != ptr->con) 
	    dest_ptr++;
	if (dest_ptr >= end_dest_ptr) {
	    dest_ptr->con = ptr->con;
	    dest_ptr->wt = wt;
	    end_dest_ptr++;
	    clusters[dest].num_in_cluster++;
	}
	else {
	    dest_ptr->wt += wt;
	}
    }

    PRINT_TRACE (12, print_qtc, clusters);

}

static int
create_new_query (in_query, out_query)
VEC *in_query;
VEC *out_query;
{
    long i;
    long num_con_wt;

    num_con_wt = in_query->num_conwt;
    for (i = 0; i < num_clusters; i++) {
	num_con_wt += clusters[i].num_in_cluster;
    }

    if (num_con_wt > max_num_con_wt) {
	if (max_num_con_wt > 0)
	    (void) free ((char *) save_con_wt);
	if (NULL == (save_con_wt = (CON_WT *)
		     malloc ((unsigned) num_con_wt * sizeof (CON_WT))))
	    return (UNDEF);
	max_num_con_wt = num_con_wt;
    }
    out_query->con_wtp = save_con_wt;

    if (in_query->num_ctype > starting_cluster_ctype) {
	set_error (SM_INCON_ERR, "q_qtc","starting_cluster_ctype too small");
	return (UNDEF);
    }
    if (num_clusters + starting_cluster_ctype > max_ctype_len) {
	if (max_ctype_len > 0)
	    (void) free ((char *) save_ctype_len);
	max_ctype_len = num_clusters + starting_cluster_ctype;
	if (NULL == (save_ctype_len = (long *)
		     malloc ((unsigned) max_ctype_len * sizeof (long))))
	    return (UNDEF);
    }
    out_query->num_ctype = num_clusters + starting_cluster_ctype;
    out_query->ctype_len = save_ctype_len;

    out_query->num_conwt = 0;
    for (i = 0; i < in_query->num_ctype; i++) {
	out_query->ctype_len[i] = in_query->ctype_len[i];
    }
    memcpy ((char *) out_query->con_wtp, 
	    (char *) in_query->con_wtp,
	    (int) in_query->num_conwt * sizeof (CON_WT));
    out_query->num_conwt = in_query->num_conwt;
    for (; i < starting_cluster_ctype; i++) {
	out_query->ctype_len[i] = 0;
    }
    for (; i < out_query->num_ctype; i++) {
	out_query->ctype_len[i] = clusters[i-starting_cluster_ctype].num_in_cluster;
	memcpy ((char *) &out_query->con_wtp[out_query->num_conwt], 
		(char *) clusters[i-starting_cluster_ctype].con_wt,
		(int) out_query->ctype_len[i] * sizeof (CON_WT));
	out_query->num_conwt += out_query->ctype_len[i];
    }

    out_query->id_num = in_query->id_num;
    return (1);
}


static float
get_corr(con, seed)
long con;
long seed;
{
    INV inv_con, inv_seed;
    LISTWT *con_ptr, *end_con_ptr;
    LISTWT *seed_ptr, *end_seed_ptr;
    long num_intersect;
    double p1, p2, p12;
    float corr;

    /* Get both inverted lists */
    inv_con.id_num = con;
    if (1 != seek_inv (inv1_fd, &inv_con) ||
	1 != read_inv (inv1_fd, &inv_con))
	return (0.0);
    inv_seed.id_num = seed;
    if (1 != seek_inv (inv2_fd, &inv_seed) ||
	1 != read_inv (inv2_fd, &inv_seed))
	return (0.0);

    /* Go through in parallel, counting intersections */
    con_ptr = inv_con.listwt;
    end_con_ptr = &inv_con.listwt[inv_con.num_listwt];
    seed_ptr = inv_seed.listwt;
    end_seed_ptr = &inv_seed.listwt[inv_seed.num_listwt];
    num_intersect = 0;
    while (con_ptr < end_con_ptr && seed_ptr < end_seed_ptr) {
	if (con_ptr->list < seed_ptr->list)
	    con_ptr++;
	else if (con_ptr->list > seed_ptr->list)
	    seed_ptr++;
	else {
	    num_intersect++;
	    con_ptr++;
	    seed_ptr++;
	}
    }

    p1 = (double) inv_con.num_listwt / num_docs_in_collection;
    p2 = (double) inv_seed.num_listwt / num_docs_in_collection;
    p12 = (double) num_intersect / num_docs_in_collection;

#define CORREL(p12,p1,p2) (p1==1.0 || p2==1.0 || p1==0.0 || p2==0.0 ? 0.0: \
                           (p12 - p1 * p2) / \
                           sqrt ((double)(p1*(1.0 - p1) * p2*(1.0 - p2))))
    corr = (float) CORREL (p12, p1, p2);
    PRINT_TRACE (12, print_float, &corr);
    return (corr);
}

static int
comp_dec_seed (p1, p2)
QTC *p1, *p2;
{
    if (p1->con_wt[0].wt > p2->con_wt[0].wt)
	return (-1);
    else if (p2->con_wt[0].wt > p1->con_wt[0].wt)
	return (1);
    return(0);
}


static void
print_qtc (qtc, output)
QTC *qtc;
SM_BUF *output;
{
    long i, j;

    for (i = 0; i < num_clusters; i++) {
	printf ("\nCluster %2ld, Seed %7ld, Num in cluster %4ld\n",
		i, qtc[i].con_wt[0].con, qtc[i].num_in_cluster);
	for (j = 0; j < qtc[i].num_in_cluster; j++) {
	    printf ("    %8ld  %7.4f\n",
		    qtc[i].con_wt[j].con,
		    qtc[i].con_wt[j].wt);
	}
    }
    fflush (stdout);
}
