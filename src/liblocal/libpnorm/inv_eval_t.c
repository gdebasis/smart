/* Evaluation of pnorm queries using an inverted file approach. 
   The query tree is traversed recursively once and each node returns
   an inverted list of documents along with the value that they evaluated
   to for the subquery starting at the node. */

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "rel_header.h"
#include "vector.h"
#include "tr.h"
#include "inv.h"
#include "pnorm_vector.h"
#include "pnorm_common.h"
#include "io.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"

#define MAX_NUM_CHILDREN  500

static PROC_TAB *t_op;
static SPEC_PARAM spec_args[] = {
    {"retrieve.t_op",	getspec_func, (char *)&t_op},
    TRACE_PARAM ("retrieve.inv_eval_t.trace")
};
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);


int
init_inv_eval_t(spec, unused)
SPEC	*spec;
char	*unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering/leaving init_inv_eval_t");
    return(0);
} /* init_inv_eval_t */


/* Evaluates the pnorm query starting at root for all documents 
   in collection. Returns an inverted list with its results */

int
inv_eval_t(query, inv_eval_t_inv_desc, root, inv_result, disjoint_val)
register	PNORM_VEC *query;
int             inv_eval_t_inv_desc;
short		root;
INV		*inv_result;
double		*disjoint_val;
{
    double q_wt[MAX_NUM_CHILDREN];	/* contains query wt of each child */

    INV child_result[MAX_NUM_CHILDREN];	/* contains the inv. list returned by
					   each child */
    int child_result_offset[MAX_NUM_CHILDREN]; /* points into child_result
						 inverted list */

    register TREE_NODE *tree_ptr;
    register OP_P_WT *op_ptr;
    long index;

    double doc_val;			/* document value */
    double xval, yval;

    long current_doc, current_con;
    int num_children, child;
    float wt;
    int i,j,size;

    double disjoint_wt[MAX_NUM_CHILDREN];/* amount that each child contributes
				    	   to the numerator when doc. has no
					   term in common with child.
					   Ex: (A,a) AND(p) (B,b) has disj. wt
					   [(1.0 - 0.0)**p] * [a ** p]  */
    int NOTflag;			/* This flag is set when NOT is used */
    
    int    inv_eval_t();

    tree_ptr = query->tree + root;
    index = tree_ptr->info;

    /* If tree is just a term then return its inverted list */
    if (UNDEF == tree_ptr->child) {   
	current_con = (query->con_wtp + index)->con;
	inv_result->id_num = current_con;
	if (1 != seek_inv(inv_eval_t_inv_desc, inv_result) ||
	    1 != read_inv(inv_eval_t_inv_desc, inv_result)) {
	    /* term not in collection, so return empty list */
	    inv_result->num_listwt = 0;
	}

	*disjoint_val = (double)0.0;
	return(1);
    }

    /* else we have an operator */
    op_ptr = query->op_p_wtp + index;

    if (op_ptr->op == NOT_OP) {
	(void)inv_eval_t(query, inv_eval_t_inv_desc, tree_ptr->child, 
			 &child_result[0], &disjoint_wt[0]);

	/* Allocate space for result */
	size = child_result[0].num_listwt;
	if (NULL == (char *)(inv_result->listwt = (LISTWT *)
			malloc(size*sizeof(LISTWT)))) {
		fprintf(stderr,"pnorm: No more space\n");
		exit(1);
	}
	inv_result->num_listwt = size;
	inv_result->id_num = UNDEF;

	for (i=0; i<size; i++) {
	    inv_result->listwt[i].list = child_result[0].listwt[i].list;
	    inv_result->listwt[i].wt = 1 - child_result[0].listwt[i].wt;
	}

	if (child_result[0].id_num == UNDEF) {
	    (void) free( (char *) child_result[0].listwt );
	}

	*disjoint_val = 1.0 - disjoint_wt[0];
	return(1);

    } /* end of processing NOT */

    /* This point is reached if operator is AND or OR */	
    num_children = 0;
    NOTflag = FALSE;

    /* Allocate space for inv_result */
    size = 4096;
    if (NULL == (char *)(inv_result->listwt = (LISTWT *)
		malloc(size*sizeof(LISTWT)))) {
	fprintf(stderr, "pnorm: No more space\n");
	exit(1);
    }
    inv_result->num_listwt = 0;
    inv_result->id_num = UNDEF;    

    /* process each child */
    for (child = tree_ptr->child; child != UNDEF;
	child = (query->tree + child)->sibling) {

	(void)inv_eval_t(query, inv_eval_t_inv_desc, 
			 child, &child_result[num_children],
			 &disjoint_wt[num_children]);
	
	/* Store the child's weight */
	if ((query->tree + child)->child == UNDEF)
	    q_wt[num_children] = (query->con_wtp+(query->tree+child)->info)->wt;
	else /* child is an operator node */
	    q_wt[num_children]=(query->op_p_wtp+(query->tree+child)->info)->wt;

	if (disjoint_wt[num_children] != 0) 
	    NOTflag = TRUE;

 	/* each child should point to the start of its inverted list */
	child_result_offset[num_children] = 0; 

	num_children++;
    }  /* end of for */

    /* Combine the inverted lists of the children */
    while (TRUE) {
	/* find next document to process */
	i=0;
	while ((i<num_children) &&
	    (child_result_offset[i] == child_result[i].num_listwt)) 
	    i++;
	if (i==num_children)
	    break;	/* we are done */

	current_doc = child_result[i].listwt[child_result_offset[i]].list;

	for (j=i+1; j<num_children; j++) {
	    if (child_result_offset[j] < child_result[j].num_listwt)
	        current_doc = MIN(current_doc,
			child_result[j].listwt[child_result_offset[j]].list);
	}

	if (op_ptr->op == AND_OP) doc_val = 1.0;
	else doc_val = 0.0;
	for (i=0; i<num_children; i++) {
	    if ((child_result_offset[i] < child_result[i].num_listwt) &&
	        (child_result[i].listwt[child_result_offset[i]].list 
							== current_doc)) {
		wt = child_result[i].listwt[child_result_offset[i]].wt;
		child_result_offset[i]++;
	    } /* if */
	    else {
		wt = disjoint_wt[i];
	    } /* else */
	    xval = doc_val;
	    yval = wt * q_wt[i];
	    (void)t_op->proc(op_ptr->op, xval, yval, op_ptr->p, &doc_val);
/*
printf ("op=%d, xval=%f, yval=%f, result=%f\n", op_ptr->op, xval, yval, doc_val);
*/
	} /* end of for */

	/* enter this document in inv_result */
	if (inv_result->num_listwt == size) {
	    size +=4096;
	    if (NULL == (inv_result->listwt = (LISTWT *)
		realloc((char *)inv_result->listwt,size*sizeof(LISTWT)))) {
		fprintf( stderr, "pnorm: No more space\n");
		exit(1);
	    }
	}

	inv_result->listwt[inv_result->num_listwt].list = current_doc;
	inv_result->listwt[inv_result->num_listwt].wt = doc_val;
	inv_result->num_listwt++;
    } /* while */

    /* Deallocate the inverted lists of children */
    for (i=0; i<num_children; i++) {
	if (child_result[i].id_num == UNDEF) {
	    (void) free((char *)child_result[i].listwt);
	}
    } /* for */

    /* compute the disjoint weight for this clause */
    if (NOTflag) {
	if (op_ptr->op == AND_OP) *disjoint_val = 1.0;
	else *disjoint_val = 0.0;
	for (i = 0; i < num_children; i++) {
	    xval = *disjoint_val;
	    yval = disjoint_wt[i] * q_wt[i];
	    (void)t_op->proc(op_ptr->op, xval, yval, op_ptr->p, disjoint_val);
	} /* for */
    }
    else 
	*disjoint_val = (double)0.0;

    return(1);

}  /* inv_eval_t */


int
close_inv_eval_t(inst)
int	inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering/leaving close_inv_eval_t");
    return(0);
} /* close_inv_eval_t */

