#ifdef RCSID
static char rcsid[] = "$Header: pconvert_wt.c,v 8.0 85/06/03 17:37:57 smart Exp $";
#endif

/* Copyright (c) 1984 - Gerard Salton, Chris Buckley, Ellen Voorhees */
/* Assign weights to the terms in a pnorm query. */
/* Assume only ctype 0 terms present */


#include <stdio.h>
#include "common.h"
#include "functions.h"
#include "inter.h"
#include "param.h"
#include "inst.h"
#include "io.h"
#include "rel_header.h"
#include "dict.h"
#include "vector.h"
#include "pnorm_vector.h"
#include "pnorm_common.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "local_functions.h"
#include "vector.h"

static long in_mode;
static long out_mode;
static char *vec_file;
static PROC_INST clause_func;
static char *textloc_file, *doc_file;
static long major_ctype;
static float pivot, slope;
static SPEC_PARAM spec_args[] = {
    {"convert.in.rmode",	getspec_filemode,  (char *)&in_mode},
    {"convert.out.rwmode",	getspec_filemode,  (char *)&out_mode},
    {"convert.vec_file",	getspec_dbfile,    (char *)&vec_file},
    {"convert.pquery_clause.weight", getspec_func, (char *)&clause_func.ptab},
    {"doc.textloc_file",	getspec_dbfile,	   (char *) &textloc_file},
    {"doc.doc_file",		getspec_dbfile,    (char *) &doc_file},
    {"convert.weight_unique.major_ctype", getspec_long, (char *) &major_ctype},
    {"convert.weight_unique.pivot", getspec_float, (char *) &pivot},
    {"convert.weight_unique.slope", getspec_float, (char *) &slope},
    TRACE_PARAM ("convert.pconvert_wt.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static long idf_inst;
static double log_num_docs;

int
init_pconvert_wt (spec, unused)
SPEC	*spec;
char	*unused;
{
    REL_HEADER *rh;

    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_pconvert_wt");

    /* Call all initialization procedures */
    if (UNDEF == (clause_func.inst =
		  clause_func.ptab->init_proc(spec, (char *)NULL)) ||
	UNDEF == (idf_inst = init_con_cw_idf(spec, "ctype.0.")))
        return (UNDEF);

    /* Find collection size for use in idf normalization */
    if (VALID_FILE (textloc_file)) {
        if (NULL == (rh = get_rel_header (textloc_file))) {
            set_error (SM_ILLPA_ERR, textloc_file, "init_pconvert_wt");
            return (UNDEF);
        }
    }
    else if (NULL == (rh = get_rel_header (doc_file))) {
        set_error (SM_ILLPA_ERR, textloc_file, "init_pconvert_wt");
        return (UNDEF);
    }
    log_num_docs = log((double) (rh->num_entries + 1));

    PRINT_TRACE (2, print_string, "Trace: leaving init_pconvert_wt");
    return (0);

} /* init_pconvert_wt */


int
pconvert_wt (in_file, out_file, inst)
char	*in_file;
char	*out_file;
int	inst;
{
    int		in_file_fd;	/* bool vector collection index */
    int		out_file_fd;	/* index of new weighted query coll */
    int		vec_fd;		/* file containing regular query vectors */
    VEC		vec;
    PNORM_VEC	in_vec;		/* current original query vector */
    PNORM_VEC	out_vec;	/* current new weighted query vector */
    register PNORM_VEC	*in_ptr;    /* pointer to bool vector */
    register PNORM_VEC	*out_ptr;   /* ptr to current weighted query */
    double mk_wts();

    PRINT_TRACE (2, print_string, "Trace: entering pconvert_wt");

    in_ptr = &in_vec;
    out_ptr = &out_vec;

    if (UNDEF == (in_file_fd=open_pnorm(in_file, in_mode)))
	return(UNDEF);

    /* Copy each vector in turn */
    in_ptr->id_num.id = 0; EXT_NONE(in_ptr->id_num.ext);
    if (global_start > 0) {
        in_ptr->id_num.id = global_start;
        if (UNDEF == seek_pnorm (in_file_fd, in_ptr)) {
            return (UNDEF);
        }
    }
    if ((UNDEF == create_pnorm(out_file)) ||
        (UNDEF == (out_file_fd=open_pnorm(out_file,out_mode))))
	return(UNDEF);
    if (UNDEF == (vec_fd = open_vector(vec_file, in_mode)))
	return(UNDEF);

    while (1 == read_pnorm(in_file_fd,in_ptr)) {
        out_ptr->id_num = in_ptr->id_num;
        out_ptr->num_ctype = in_ptr->num_ctype;
        out_ptr->ctype_len = in_ptr->ctype_len;
        out_ptr->num_conwt = in_ptr->num_conwt;
	vec.id_num = in_ptr->id_num;
	if (UNDEF == seek_vector(vec_fd, &vec) ||
	    1 != read_vector(vec_fd, &vec))
	    return(UNDEF);
        if (NULL == (char *)(out_ptr->con_wtp = (CON_WT *)
            malloc((unsigned)in_ptr->num_conwt*sizeof(CON_WT)))) {
            print_error ("pconvert_wt", "Quit");
	    return(UNDEF);
        }
        out_ptr->num_nodes = in_ptr->num_nodes;
        out_ptr->tree = in_ptr->tree;
        out_ptr->num_op_p_wt = in_ptr->num_op_p_wt;
        if (NULL == (char *)(out_ptr->op_p_wtp = (OP_P_WT *)
            malloc((unsigned)in_ptr->num_op_p_wt*sizeof(OP_P_WT)))) {
            print_error ("pconvert_wt", "Quit");
	    return(UNDEF);
        }

        (void)mk_wts(in_ptr,out_ptr,(short)0, &vec);

        if (0 != seek_pnorm(out_file_fd,out_ptr) ||
            1 != write_pnorm(out_file_fd,out_ptr)  ) {
            print_error ("pconvert_wt", "Quit");
	    return(UNDEF);
        }

        (void)free((char *)out_ptr->op_p_wtp);
        (void)free((char *)out_ptr->con_wtp);
    } /* while */

    (void)close_pnorm(in_file_fd);
    (void)close_pnorm(out_file_fd);
    (void)close_vector(vec_fd);

    PRINT_TRACE (2, print_string, "Trace: leaving pconvert_wt");
    return (1);

} /* pconvert_wt */


int
close_pconvert_wt (inst)
int	inst;
{
    if (UNDEF == clause_func.ptab->close_proc(clause_func.inst) ||
	UNDEF == close_con_cw_idf(idf_inst))
	return(UNDEF);

    PRINT_TRACE (2,print_string, "Trace: entering/leaving close_pconvert_wt");
    return (0);
} /* close_pconvert_wt */


double
mk_wts(in_ptr, out_ptr, root, wt_vec)
register PNORM_VEC *in_ptr;	/* original query ptr */
register PNORM_VEC *out_ptr;	/* weighted query ptr */
short root;			/* root of current (sub)tree */
VEC *wt_vec;			/* regular vector from which the weights are
				 * taken */
{
    register TREE_NODE	*in_tree_ptr;
    double		wt;
    long		offset;
    long		con;
    float		idf, sum;
    CON_WT		*cwp;

    PRINT_TRACE (2,print_string, "Trace: entering mk_wts");

    in_tree_ptr = in_ptr->tree + root;
    offset = in_tree_ptr->info;

    if (UNDEF == in_tree_ptr->child) {	/* a concept */
	con = (in_ptr->con_wtp+offset)->con;
        (out_ptr->con_wtp+offset)->con = con;
	for (cwp = wt_vec->con_wtp; 
	     cwp < wt_vec->con_wtp + wt_vec->ctype_len[0] &&
		 cwp->con != con;
	     cwp++);
	if (cwp < wt_vec->con_wtp + wt_vec->ctype_len[0])
	    wt = cwp->wt;
	else { 
	    if (UNDEF == con_cw_idf(&con, &idf, idf_inst))
		return(UNDEF);
	    idf /= log_num_docs;
	    if (-1 == major_ctype)
		sum = wt_vec->num_conwt;
	    else if (major_ctype < wt_vec->num_ctype)
		sum = wt_vec->ctype_len[major_ctype];
	    else {
		set_error(SM_INCON_ERR, "Bad major_ctype", "pconvert_wt");
		return (UNDEF);
	    }
   	    wt = idf / (1 + (slope / (1.0 - slope)) * (sum/pivot));
	}
        (out_ptr->con_wtp+offset)->wt = wt;
    } /* if */
    else {				/* an operator node */
	if (UNDEF == clause_func.ptab->proc(in_ptr, out_ptr, in_tree_ptr,
					    &wt, wt_vec))
	    return (UNDEF);

        (out_ptr->op_p_wtp+offset)->op = (in_ptr->op_p_wtp+offset)->op;
        (out_ptr->op_p_wtp+offset)->p  = (in_ptr->op_p_wtp+offset)->p;
        (out_ptr->op_p_wtp+offset)->wt = (float)wt;
    } /* else */

    PRINT_TRACE (2,print_string, "Trace: leaving mk_wts");
    return(wt);
} /* mk_wts */


/* average of children's weights */
int
pclause_avg(in_ptr, out_ptr, in_tree_ptr, wt, wt_vec)
register PNORM_VEC	*in_ptr;	
register PNORM_VEC	*out_ptr;	
register TREE_NODE	*in_tree_ptr;
double			*wt;
VEC 			*wt_vec;
{
    register int	child;
    int			num_children;
    double		mk_wts();
    double		wttmp;

    PRINT_TRACE (2, print_string, "Trace: entering pclause_avg");

    num_children = 0;
    *wt = 0.0;
    for (child = in_tree_ptr->child; UNDEF != child;
	child = (in_ptr->tree+child)->sibling) {
	num_children++;
        if (UNDEF == (wttmp=mk_wts(in_ptr,out_ptr,(short)child, wt_vec)))
	    return(UNDEF);
	*wt += wttmp;
    }
    *wt /= num_children;

    PRINT_TRACE (2, print_string, "Trace: leaving pclause_avg");
    return(1);
} /* pclause_avg */


int
pclause_sw(in_ptr, out_ptr, in_tree_ptr, wt, wt_vec)
register PNORM_VEC	*in_ptr;	
register PNORM_VEC	*out_ptr;	
register TREE_NODE	*in_tree_ptr;
double			*wt;
VEC			*wt_vec;
{
    register int	child;
    OP_P_WT		*op_ptr;
    int			num_children;
    double		mk_wts();
    double		wttmp;

    PRINT_TRACE (2, print_string, "Trace: entering pclause_sw");

    op_ptr = in_ptr->op_p_wtp + in_tree_ptr->info;

    num_children = 0;
    *wt = 0.0;
    for (child = in_tree_ptr->child; UNDEF != child;
	child = (in_ptr->tree+child)->sibling) {
	num_children++;
        if (UNDEF == (wttmp=mk_wts(in_ptr,out_ptr,(short)child, wt_vec)))
	    return(UNDEF);
	*wt += wttmp;
    }
    if (op_ptr->op != AND_OP)
	*wt /= num_children;

    PRINT_TRACE (2, print_string, "Trace: leaving pclause_sw");
    return(1);
} /* pclause_sw */
