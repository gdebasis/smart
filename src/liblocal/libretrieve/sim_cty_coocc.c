#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libretrieve/sim_cty.c,v 11.2 1993/02/03 16:52:55 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Perform inverted retrieval algorithm for cooccurrences
 *1 local.retrieve.ctype_coll.ctype_coocc
 *2 sim_ctype_coocc (qvec, results, inst)
 *3   VEC *qvec;
 *3   RESULT *results;
 *3   int inst;
 *4 init_sim_ctype_coocc (spec, param_prefix)
 *5   "retrieve.rank_tr"
 *5   "retrieve.ctype_coll.trace"
 *5   "ctype.*.inv_file"
 *5   "ctype.*.inv_file.rmode"
 *5   "ctype.*.sim_ctype_weight"
 *4 close_sim_ctype_coocc (inst)

 *7 Compute inner-product similarity for ctype 2 (cooccurring term pairs). 
 *7 See libretrieve/sim_cty_inv.c for basic structure.
 *7 Outline: 
 *7	Look up current concept number in dictionary to get the 
 *7		corresponding pair of terms
 *7	Get inverted lists for this pair and find the common docs
 *7	Increment the similarity for the common docs
***********************************************************************/

#include "common.h"
#include "context.h"
#include "functions.h"
#include "inst.h"
#include "inv.h"
#include "param.h"
#include "proc.h"
#include "rel_header.h"
#include "retrieve.h"
#include "smart_error.h"
#include "spec.h"
#include "tr_vec.h"
#include "trace.h"
#include "vector.h"

static PROC_TAB *rank_tr;
static char *inv_file;
static long inv_mode;
static PROC_TAB *tokcon;
static SPEC_PARAM spec_args[] = {
    {"retrieve.rank_tr",	getspec_func,	(char *) &rank_tr},
    {"ctype.0.inv_file",	getspec_dbfile,	(char *) &inv_file},
    {"ctype.0.inv_file.rmode",	getspec_filemode, (char *) &inv_mode},
    {"retrieve.coocc.ctype.0.token_to_con", getspec_func, (char *) &tokcon},
    TRACE_PARAM ("retrieve.ctype_coll.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static char *prefix;
static float ctype_weight;
static SPEC_PARAM_PREFIX pspec_args[] = {
    {&prefix,  "sim_ctype_weight", getspec_float, (char *) &ctype_weight},
    };
static int num_pspec_args = sizeof (pspec_args) / sizeof (pspec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;
    int contok_inst, tokcon_inst, rank_tr_inst;
    /* ctype 0 inverted file info */
    int inv_fd;
    float ctype_weight;                /* Weight for this particular ctype */
} STATIC_INFO;
static STATIC_INFO *info;
static int max_inst = 0;

static int comp_q_wt();
int init_contok_dict_noinfo(), contok_dict_noinfo(), close_contok_dict_noinfo();

int
init_sim_ctype_coocc (spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    STATIC_INFO *ip;
    int new_inst;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);
    prefix = param_prefix;
    if (UNDEF == lookup_spec_prefix (spec, &pspec_args[0], num_pspec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_sim_ctype_coocc");
    if (! VALID_FILE (inv_file)) {
        set_error (SM_ILLPA_ERR, "Non-existant inverted file", "sim_ctype_coocc");
        return (UNDEF);
    }

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];

    /* Call all initialization procedures */
    if (UNDEF == (ip->contok_inst = init_contok_dict_noinfo(spec, param_prefix)) ||
	UNDEF == (ip->tokcon_inst = tokcon->init_proc(spec, "index.section.word.")) ||
	UNDEF == (ip->rank_tr_inst = rank_tr->init_proc(spec, NULL))) {
        return (UNDEF);
    }

    /* Open inverted file */
    if (UNDEF == (ip->inv_fd = open_inv(inv_file, inv_mode)))
        return (UNDEF);

    ip->ctype_weight = ctype_weight;
    ip->valid_info++;

    PRINT_TRACE (2, print_string, "Trace: leaving init_sim_ctype_coocc");
    return (new_inst);
}

int
sim_ctype_coocc (qvec, results, inst)
VEC *qvec;
RESULT *results;
int inst;
{
    STATIC_INFO *ip;
    CON_WT *query_con;
    CON_WT *last_query_con;
    register float query_wt;
    TOP_RESULT new_top;
    VEC new_qvec;

    char term_buf[PATH_LEN], *term_pair, *term1, *term2;
    long con1, con2;
    INV inv1, inv2;
    register LISTWT *lwp1, *end1, *lwp2, *end2;

    PRINT_TRACE (2, print_string, "Trace: entering sim_ctype_coocc");
    PRINT_TRACE (6, print_vector, qvec);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "sim_ctype_coocc");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Copy query, and sort so that all negative query weights are first
       (so they don't ever enter top docs) and then by decreasing
       query weight (so fewer docs enter top docs) */
    if (UNDEF == copy_vec (&new_qvec, qvec))
        return (UNDEF);
    qsort ((char *) new_qvec.con_wtp,
           (int) new_qvec.num_conwt, 
           sizeof (CON_WT),
           comp_q_wt);

    /* Perform inverted file retrieval, sequentially going through query.
       Ignore ctype (qvec probably is only one ctype, either naturally or
       because this invoked through sim_vec_inv) */
    last_query_con = &new_qvec.con_wtp[new_qvec.num_conwt];
    for (query_con = new_qvec.con_wtp; 
	 query_con < last_query_con;
	 query_con++) {

        /* Weight query term by ctype weight.  Skip this term if new query */
        /* weight is 0 */
        if (0.0 == (query_wt = ip->ctype_weight * query_con->wt)) {
            continue;
        }

	/* Get the term-pair corresponding to this ctype 2 term */
	if (UNDEF == contok_dict_noinfo(&(query_con->con), &term_pair, 
					ip->contok_inst))
	    return(UNDEF);
	strcpy(term_buf, term_pair);
	term2 = term1 = term_buf;
	while (*term2 != ' ') term2++;
	*term2 = '\0';
	term2++;
	if (UNDEF == tokcon->proc(term1, &con1, ip->tokcon_inst) ||
	    UNDEF == tokcon->proc(term2, &con2, ip->tokcon_inst))
	    return(UNDEF);

	/* Get the inverted lists corresponding to the 2 terms */
	inv1.id_num = con1;
        if (1 != seek_inv(ip->inv_fd, &inv1) ||
            1 != read_inv(ip->inv_fd, &inv1)) {
            continue;
        }
	inv2.id_num = con2;
        if (1 != seek_inv(ip->inv_fd, &inv2) ||
            1 != read_inv(ip->inv_fd, &inv2)) {
            continue;
        }

	/* Add similarity for common docs */
	end1 = &inv1.listwt[inv1.num_listwt];	
	end2 = &inv2.listwt[inv2.num_listwt];
        for (lwp1 = inv1.listwt, lwp2 = inv2.listwt;
             lwp1 < end1 && lwp2 < end2;) {
	    /* find common docs */
	    if (lwp1->list < lwp2->list) {
		lwp1++; continue;
	    }
	    if (lwp1->list > lwp2->list) {
		lwp2++;
		continue;
	    }
	    /* Ensure that did does not exceed bounds for partial similarity*/
            /* array.  Realloc if necessary */
            if (lwp1->list >= results->num_full_results) {
                if (NULL == (results->full_results = (float *)
			     realloc((char *) results->full_results,
				     (unsigned)(lwp1->list+MAX_DID_DEFAULT) *
				     sizeof (float)))) {
                    set_error(errno, "Realloc partial sim", "sim_ctype_coocc");
                    return (UNDEF);
                }
                bzero((char *) (&results->full_results[results->num_full_results]),
		      (int) (lwp1->list + MAX_DID_DEFAULT -
			     results->num_full_results) * sizeof (float));
                results->num_full_results = lwp1->list + MAX_DID_DEFAULT;
            }

            results->full_results[lwp1->list] += query_wt*MIN(lwp1->wt, lwp2->wt);
            /* Add to top results (potentially) if new sim exceeds top_thresh */
            /* (new_top points to last entry in top_results subarray of */
            /* results) */
            if (results->full_results[lwp1->list] >=
                results->min_top_sim) {
                new_top.sim = results->full_results[lwp1->list];
                new_top.did = lwp1->list;
                if (UNDEF == rank_tr->proc (&new_top,
                                            results,
                                            ip->rank_tr_inst))
                    return (UNDEF);
            }
	    lwp1++; lwp2++;
        }
    }
    
    PRINT_TRACE (20, print_full_results, results);
    PRINT_TRACE (4, print_top_results, results);
    PRINT_TRACE (2, print_string, "Trace: leaving sim_ctype_coocc");
    return (1);
}

int
close_sim_ctype_coocc (inst)
int inst;
{
    STATIC_INFO *ip;
    PRINT_TRACE (2, print_string, "Trace: entering close_sim_ctype_coocc");
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "ctype_coocc");
        return (UNDEF);
    }
    ip  = &info[inst];
    ip->valid_info--;

    if (ip->valid_info == 0) {
        if (UNDEF == close_contok_dict_noinfo(ip->contok_inst) ||
	    UNDEF == tokcon->close_proc(ip->tokcon_inst) ||
	    UNDEF == rank_tr->close_proc(ip->rank_tr_inst))
            return (UNDEF);
        if (UNDEF == close_inv(ip->inv_fd))
            return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_sim_ctype_coocc");
    return (0);
}


/* All negative wts first, then by decreasing weight */
static int
comp_q_wt (ptr1, ptr2)
CON_WT *ptr1;
CON_WT *ptr2;
{
    if (ptr1->wt < 0.0) {
        if (ptr2->wt >= 0.0)
            return (-1);
        return (0);
    }
    if (ptr2->wt < 0.0)
        return (1);
    if (ptr1->wt < ptr2->wt)
        return (1);
    if (ptr1->wt > ptr2->wt)
        return (-1);
    return (0);
}
