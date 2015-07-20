#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfeedback/exp_nr.c,v 11.2 1993/02/03 16:49:57 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "feedback.h"
#include "functions.h"
#include "inv.h"
#include "param.h"
#include "spec.h"
#include "tr_vec.h"
#include "trace.h"
#include "vector.h"
#include "io.h"


/* Expand query by adding all terms that occur non-randomly in rel docs.
 * and, in addition, occur in documents that cluster.
 * Query terms are always in the new query.
 * Handles single terms (word, ctype 0), phrases (phrase, ctype 1). 
 * Similar to exp_nonrand.c
 */

static char *cluster_file;
static SPEC_PARAM spec_args[] = {
    {"feedback.cluster_file",	getspec_dbfile,	(char *)&cluster_file},
    TRACE_PARAM ("feedback.expand.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static char *prefix;
static long min_percent;	/* Cutoff for random occurrences */
static SPEC_PARAM_PREFIX prefix_spec_args[] = {
    {&prefix,	"min_percent",	getspec_long,	(char *) &min_percent},
};
static int num_prefix_spec_args = 
	sizeof (prefix_spec_args) / sizeof (prefix_spec_args[0]);

static int exp_rel_doc_inst, cluster_fd;
static SPEC *save_spec;

static int in_cluster(), compare_con(), compare_occ_info();
int init_local_exp_reldoc(), local_exp_reldoc(), close_local_exp_reldoc();


int
init_exp_cluster (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_exp_cluster");

    if (UNDEF == (exp_rel_doc_inst = init_local_exp_reldoc(spec, (char *) NULL)))
        return (UNDEF);
    if (UNDEF == (cluster_fd = open_vector(cluster_file, SRDONLY)))
	return(UNDEF);

    save_spec = spec;

    PRINT_TRACE (2, print_string, "Trace: leaving init_exp_cluster");
    return (1);
}


int
exp_cluster (unused, fdbk_info, inst)
char *unused;
FEEDBACK_INFO *fdbk_info;
int inst;
{
    char param_prefix[40];
    long num_orig_query, num_this_ctype, min_rel, i;
    OCC *start_ctype_occ, *end_ctype_occ, *end_occ, *end_good_occ, *occp;
    VEC cvec, ctype_vec;

    PRINT_TRACE (2, print_string, "Trace: entering exp_cluster");

    /* Get all terms (ctypes 0 and 1) occurring in rel docs */
    if (UNDEF == local_exp_reldoc (unused, fdbk_info, exp_rel_doc_inst))
        return (UNDEF);

    if (fdbk_info->num_rel > 0) {
        PRINT_TRACE (8, print_fdbk_info, fdbk_info);
	/* Get cluster centroid vector */
	cvec.id_num = fdbk_info->orig_query->id_num;
	if (UNDEF == seek_vector(cluster_fd, &cvec) ||
	    UNDEF == read_vector(cluster_fd, &cvec))
	    return(UNDEF);

        /* Go through expanded terms ctype by ctype keeping all "non-randomly
	 * occurring" terms that are included in centroid vectors */
        start_ctype_occ = fdbk_info->occ;
        end_good_occ = fdbk_info->occ;
        end_occ = &fdbk_info->occ[fdbk_info->num_occ];
        while (start_ctype_occ < end_occ) {
            end_ctype_occ = start_ctype_occ + 1;
            num_orig_query = (start_ctype_occ->query ? 1 : 0);
            while (end_ctype_occ < end_occ &&
                   end_ctype_occ->ctype == start_ctype_occ->ctype) {
                if (end_ctype_occ->query)
                    num_orig_query++;
                end_ctype_occ++;
            }
            num_this_ctype = end_ctype_occ - start_ctype_occ;

	    /* HACK: if a term does not occur in a centroid vector,
	     * set its rel_ret to 0. This will ensure that such terms
	     * are not included in the final fdbk_info.
	     */
	    ctype_vec.con_wtp = cvec.con_wtp;
	    for (i = 0; i < start_ctype_occ->ctype; i++)
		ctype_vec.con_wtp += cvec.ctype_len[i];
	    ctype_vec.num_conwt = cvec.ctype_len[start_ctype_occ->ctype];
	    for (occp = start_ctype_occ; occp < end_ctype_occ; occp++)
		if (!in_cluster(occp->con, &ctype_vec) && !occp->query)
		    occp->rel_ret = 0;

            /* Sort the occurrence info by
             *  1. query (all original query terms occur first)
             *  2. rel_ret (those terms occurring in most rel docs occur next)
             */
            qsort((char *) start_ctype_occ, num_this_ctype, sizeof (OCC),
		  compare_occ_info);

            sprintf (param_prefix, "feedback.ctype.%ld.",
		     start_ctype_occ->ctype);
            prefix = param_prefix;
            if (UNDEF == lookup_spec_prefix (save_spec,
                                             &prefix_spec_args[0],
                                             num_prefix_spec_args))
                return (UNDEF);
	    min_rel = (long) 
		floor((double)(fdbk_info->num_rel * min_percent)/100.0 + 0.5);

	    /* Ignore terms which occur in < min_percent of the rel. docs */
	    occp = start_ctype_occ + num_orig_query;
	    while (occp < end_ctype_occ && occp->rel_ret >= min_rel)
		occp++;
	    num_this_ctype = occp - start_ctype_occ;
            /* Resort by con */
            qsort((char *) start_ctype_occ, num_this_ctype, sizeof(OCC), 
		  compare_con);
            /* Copy into good portion of occ array (skip copying if already
               in correct location) */
            if (start_ctype_occ != end_good_occ) {
                (void) bcopy ((char *) start_ctype_occ,
                              (char *) end_good_occ,
                              sizeof (OCC) * (int) num_this_ctype);
            }
            end_good_occ += num_this_ctype;
            start_ctype_occ = end_ctype_occ;
        }
	fdbk_info->num_occ = end_good_occ - fdbk_info->occ;
    }

    PRINT_TRACE (4, print_fdbk_info, fdbk_info);
    PRINT_TRACE (2, print_string, "Trace: leaving exp_cluster");
    return (1);
}


int
close_exp_cluster (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_exp_cluster");

    if (UNDEF == close_local_exp_reldoc(exp_rel_doc_inst))
        return (UNDEF);
    if (UNDEF == close_vector(cluster_fd))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_exp_cluster");
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

static int
compare_occ_info (ptr1, ptr2)
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
    if (ptr1->rel_ret > ptr2->rel_ret)
        return (-1);
    if (ptr1->rel_ret < ptr2->rel_ret)
        return (1);
    return (0);
}
