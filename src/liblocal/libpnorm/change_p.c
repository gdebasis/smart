#ifdef RCSID
static char rcsid[] = "$Header: change_p.c,v 8.0 85/06/03 17:37:46 smart Exp $";
#endif

/* Copyright (c) 1984 - Gerard Salton, Chris Buckley, Ellen Voorhees */

#include <stdio.h>
#include "common.h"
#include "functions.h"
#include "param.h"
#include "io.h"
#include "rel_header.h"
#include "vector.h"
#include "pnorm_vector.h"
#include "pnorm_common.h"
#include "spec.h"
#include "trace.h"
#include "local_functions.h"

/* change p values for AND and OR operations in pnorm vectors */

static long in_mode;
static long out_mode;
static float and_p;
static float or_p;

static SPEC_PARAM spec_args[] = {
    {"convert.in.rmode",     getspec_filemode,  (char *) &in_mode},
    {"convert.out.rwmode",   getspec_filemode,  (char *) &out_mode},
    {"convert.pval.and_p",   getspec_float,     (char *) &and_p},
    {"convert.pval.or_p",    getspec_float,     (char *) &or_p},
    TRACE_PARAM ("convert.change_p.trace")
    };
static int num_spec_args =
    sizeof (spec_args) / sizeof (spec_args[0]);


int
init_change_p (spec, unused)
SPEC	*spec;
char	*unused;
{
    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering/leaving init_change_p");
    return (0);

} /* init_change_p */


int
change_p (in_file, out_file, inst)
char	*in_file;
char	*out_file;
int	inst;
{
    int in_file_idx;			/* index to original query coll */
    int out_file_idx;			/* index of new-p query coll */
    PNORM_VEC in_vector;		/* current original query vector */
    PNORM_VEC out_vector;		/* current new-p query vector */
    register PNORM_VEC *in_ptr;		/* pointer to orig vector */
    register PNORM_VEC *out_ptr;	/* pointer to new-p vector */
    OP_P_WT op_tuples[MAX_VEC];		/* new operator tuples */
    int i;

    PRINT_TRACE (2, print_string, "Trace: entering change_p");

    in_ptr = &in_vector;
    out_ptr = &out_vector; 

    if (UNDEF == (in_file_idx=open_pnorm(in_file,in_mode)))
	return(UNDEF);

    if ((UNDEF == seek_pnorm(in_file_idx,(PNORM_VEC *)NULL)) ||
        (UNDEF == read_pnorm(in_file_idx,in_ptr)))
	return(UNDEF);

    if ((UNDEF == create_pnorm(out_file)) ||
        (UNDEF == (out_file_idx=open_pnorm(out_file,out_mode))))
	return(UNDEF);

    /* create a new p query vector for each original p query */
    if (UNDEF == seek_pnorm(in_file_idx,(PNORM_VEC *)NULL))
	return(UNDEF);

    while (1 == read_pnorm(in_file_idx,in_ptr)) {
        out_ptr->id_num      = in_ptr->id_num;
        out_ptr->num_conwt   = in_ptr->num_conwt;
        out_ptr->con_wtp     = in_ptr->con_wtp;
        out_ptr->num_ctype   = in_ptr->num_ctype;
        out_ptr->ctype_len   = in_ptr->ctype_len;
        out_ptr->num_nodes   = in_ptr->num_nodes;
        out_ptr->tree        = in_ptr->tree;
        out_ptr->num_op_p_wt = in_ptr->num_op_p_wt;
        if (out_ptr->num_op_p_wt > MAX_VEC) {
            fprintf(stderr,"change_p: too many operator tuples. Quit.\n");
	    return(UNDEF);
        }
        out_ptr->op_p_wtp = &op_tuples[0];
        bcopy((char *)in_ptr->op_p_wtp, (char *)&(op_tuples[0]),
	      out_ptr->num_op_p_wt * sizeof(OP_P_WT));
        for (i=0; i<out_ptr->num_op_p_wt; i++) {
            switch (op_tuples[i].op) {
            case AND_OP:
                op_tuples[i].p = and_p;
                break;
            case OR_OP:
                op_tuples[i].p = or_p;
                break;
            case NOT_OP:
                break;
            default:
                fprintf(stderr,"change_p: unknown operator type %d\n",
                      op_tuples[i].op);
                return(UNDEF);
            } /* switch */
        } /* for */
        if (0 != seek_pnorm(out_file_idx,out_ptr) ||
            1 != write_pnorm(out_file_idx,out_ptr) ) {
            print_error ("change_p", "Quit");
	    return(UNDEF);
        }
    } /* while */

    (void)close_pnorm(in_file_idx);
    (void)close_pnorm(out_file_idx);

    PRINT_TRACE (2, print_string, "Trace: leaving change_p");
    return (1);

} /* change_p */


int
close_change_p (inst)
int	inst;
{
    PRINT_TRACE (2,print_string, "Trace: entering/leaving close_change_p");
    return (0);
} /* close_change_p */




