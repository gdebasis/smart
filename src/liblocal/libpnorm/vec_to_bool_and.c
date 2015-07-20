#ifdef RCSID
static char rcsid[] = "$Header: vec_to_bool_and.c,v 8.0 85/06/03 17:37:46 smart Exp $";
#endif

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

/* This program transforms a vector query into a flat pnorm query by
   simply ANDing all of the terms and using pvalue 1.0 */

#define MAX_TREE_SIZE 2000

static long in_mode;
static long out_mode;

static SPEC_PARAM spec_args[] = {
    {"convert.in.rmode",     getspec_filemode,  (char *) &in_mode},
    {"convert.out.rwmode",   getspec_filemode,  (char *) &out_mode},
    TRACE_PARAM ("convert.vec_to_bool_and.trace")
    };
static int num_spec_args =
    sizeof (spec_args) / sizeof (spec_args[0]);


int
init_vec_to_bool_and (spec, unused)
SPEC	*spec;
char	*unused;
{
    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE(2,print_string,"Trace: entering/leaving init_vec_to_bool_and");
    return (0);

} /* init_vec_to_bool_and */


int
vec_to_bool_and (vec_name, pvec_name, inst)
char	*vec_name;
char	*pvec_name;
int	inst;
{
    PNORM_VEC	pvec;
    VEC		vec;
    int		fd_vec, fd_pvec;
    int i;

    PRINT_TRACE (2, print_string, "Trace: entering vec_to_bool_and");

    /* open files */
    if (UNDEF == (fd_vec = open_vector(vec_name, in_mode)) ||
        UNDEF == create_pnorm(pvec_name) ||
        UNDEF == (fd_pvec = open_pnorm(pvec_name, out_mode))) {
        fprintf(stderr, "Can't open files\n");
        return(UNDEF);
    }

    /* allocate space for pvector */
    if (NULL == (char *) (pvec.tree = 
        (TREE_NODE *)malloc(MAX_TREE_SIZE * sizeof(TREE_NODE))) ||
        NULL == (char *) (pvec.op_p_wtp = 
        (OP_P_WT *)malloc(1 * sizeof(OP_P_WT)))) {
        fprintf(stderr,"Can't allocate space\n");
        return(UNDEF);
    }

    pvec.num_op_p_wt = 1;
    pvec.op_p_wtp->op = AND_OP;
    pvec.op_p_wtp->p = 1.0;
    pvec.op_p_wtp->wt = 1.0;
    pvec.tree->info = 0;
    pvec.tree->child = 1;
    pvec.tree->sibling = UNDEF;

    while (1 == read_vector(fd_vec, &vec)) {
	pvec.id_num = vec.id_num;
	pvec.num_ctype = vec.num_ctype;
	pvec.num_conwt = vec.num_conwt;
	pvec.num_nodes = vec.num_conwt + 1;
	pvec.ctype_len = vec.ctype_len;
	pvec.con_wtp = vec.con_wtp;

	for (i=0; i<vec.num_conwt; i++) {
	    (pvec.tree + i + 1)->info = i;
	    (pvec.tree + i + 1)->child = UNDEF;
	    (pvec.tree + i + 1)->sibling = i+2;
	} /* for */
	(pvec.tree + vec.num_conwt)->sibling = UNDEF;

	if (0 != seek_pnorm( fd_pvec, &pvec ) ||
	    1 != write_pnorm( fd_pvec, &pvec )) {
		fprintf(stderr, "Write error\n");
		return(UNDEF);
	}
    } /* end of while */

    close_vector(fd_vec);
    close_pnorm(fd_pvec);

    PRINT_TRACE (2, print_string, "Trace: leaving vec_to_bool_and");
    return (1);
} /* vec_to_bool_and */


int
close_vec_to_bool_and (inst)
int	inst;
{
    PRINT_TRACE(2,print_string,"Trace: entering/leaving close_vec_to_bool_and");
    return (0);
} /* close_vec_to_bool_and */
