/* Evaluation of pnorm queries using an inverted file approach. 
   The query tree is traversed recursively once and each node returns
   an inverted list of documents along with the value that they evaluated
   to for the subquery starting at the node. by Maria Smith 9/17/87 */

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

#define MAX_NUM_CHILDREN  500

/*  THIS FILE NEEDS TO BE CHANGED TO PUT IT INTO SMART PROCEDURE FORMAT */

/* Evaluates the pnorm query starting at root for all documents 
   in collection. Returns an inverted list with its results */

int
inv_eval_p(query, inv_eval_p_inv_desc, root, inv_result, disjoint_val)
register	PNORM_VEC *query;
int             inv_eval_p_inv_desc;
short		root;
INV		*inv_result;
double		*disjoint_val;
{
    double q_wt[MAX_NUM_CHILDREN];	/* contains query wt of each child */
    double q_wt_exp[MAX_NUM_CHILDREN];	/* contains query wt of each child
					   raised to the power p */

    INV child_result[MAX_NUM_CHILDREN];	/* contains the inv. list returned by
					   each child */
    LISTWT *tmpinv;
    int child_result_offset[MAX_NUM_CHILDREN]; /* points into child_result
						 inverted list */

    register TREE_NODE *tree_ptr;
    register OP_P_WT *op_ptr;
    long index;

    register double numerator;
    register double denominator;

    long current_doc, current_con;
    int num_children, child;
    double power;
    float wt;
    int i,j,size;

    double disjoint_wt[MAX_NUM_CHILDREN];/* amount that each child contributes
				    	   to the numerator when doc. has no
					   term in common with child.
					   Ex: (A,a) AND(p) (B,b) has disj. wt
					   [(1.0 - 0.0)**p] * [a ** p]  */
    double disjoint_numerator;		/* used to compute what this tree 
					   starting at root evaluates to when
					   a document has no term in common
					   with it. Normally zero, except
					   when the NOT operator is used */
    int NOTflag;			/* This flag is set when NOT is used */


    double eval_p_formula();
    double robust_pow();
    int    inv_eval_p();

    tree_ptr = query->tree + root;
    index = tree_ptr->info;
    denominator = 0.0;

    /* If tree is just a term then return its inverted list */
    if (UNDEF == tree_ptr->child) {   
	current_con = (query->con_wtp + index)->con;
	inv_result->id_num = current_con;
	if (1 != seek_inv(inv_eval_p_inv_desc, inv_result) ||
	    1 != read_inv(inv_eval_p_inv_desc, inv_result)) {
	    /* term not in collection, so return empty list */
	    inv_result->num_listwt = 0;
	}
	/* Copy inv list to a safe place since no guarantee that
	 * later read_invs will not reuse this space.
	 */
	if (NULL == (tmpinv = Malloc(inv_result->num_listwt, LISTWT))) {
	    set_error(0, "Out of space", "inv_eval_p");
	    return(UNDEF);
	}
	bcopy((char *)inv_result->listwt, (char *)tmpinv, 
	      inv_result->num_listwt * sizeof(LISTWT));
	inv_result->listwt = tmpinv;

#ifdef PNORMDEBUG
{ int i;
    printf ("***********************************\n");
    for (i = 0; i < inv_result->num_listwt; i++) {
        (void) printf ("%ld\t%d\t%ld\t%f\n",
                        inv_result->id_num,
                        (int) 0,
                        inv_result->listwt[i].list,
                        inv_result->listwt[i].wt);
    }
    printf ("***********************************\n");
}
#endif

	*disjoint_val = (double)0.0;
	return(1);
    }

    /* else we have an operator */
    op_ptr = query->op_p_wtp + index;

    if (op_ptr->op == NOT_OP) {
	(void)inv_eval_p(query, inv_eval_p_inv_desc, tree_ptr->child, 
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
    disjoint_numerator = 0.0;
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

	(void)inv_eval_p(query, inv_eval_p_inv_desc,
			 child, &child_result[num_children],
			 &disjoint_wt[num_children]);
	
	/* Store the child's weight and its weight raised to the power p */
	if ((query->tree + child)->child == UNDEF)
	    q_wt[num_children] = (query->con_wtp+(query->tree+child)->info)->wt;
	else /* child is an operator node */
	    q_wt[num_children]=(query->op_p_wtp+(query->tree+child)->info)->wt;

	if (op_ptr->p != P_INFINITY) {
	    if ((double)UNDEF == (power=robust_pow(q_wt[num_children],
						    (double)op_ptr->p)))
		exit(4);
	    q_wt_exp[num_children] = power;
	    denominator += power;
	}
	else
	    denominator = MAX(denominator,q_wt[num_children]);

	if (disjoint_wt[num_children] != 0) 
	    NOTflag = TRUE;

 	/* each child should point to the start of its inverted list */
	child_result_offset[num_children] = 0; 

	num_children++;
    }  /* end of for */

    /* Compute contribution of each child to the numerator for a "disjoint"
       document */
    if (NOTflag) {
	for (i=0; i<num_children; i++) {
	    if ((disjoint_wt[i] == 0) && (op_ptr->op == OR_OP) ) 
		continue;
	    if (op_ptr->op == AND_OP)
		wt = (1.0 - disjoint_wt[i]) * q_wt[i];
	    else 
		wt = disjoint_wt[i] * q_wt[i];
	    if (op_ptr->p != P_INFINITY)  {
		if ((double)UNDEF == 
		(power = robust_pow((double)wt,(double)op_ptr->p)))
		   exit(5);
		disjoint_wt[i] = power;
		disjoint_numerator += power;
	    }
	    else {
		disjoint_wt[i] = wt;
		disjoint_numerator = MAX(disjoint_numerator,wt);
	    }
	} /* for */
    }
    /* AND always has a disjoint contribution to the numerator */
    if (!NOTflag && op_ptr->op == AND_OP) 
	for (i=0; i<num_children; i++) {
	    if (op_ptr->p != P_INFINITY)
	   	disjoint_wt[i] = q_wt_exp[i];
	    else
		disjoint_wt[i] = q_wt[i];
	} /* for */
	   
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

#ifdef PNORMDEBUG
printf ("current_doc=%ld\n", current_doc);
#endif

	numerator = 0.0;

	for (i=0; i<num_children; i++) {
	    if ((child_result_offset[i] < child_result[i].num_listwt) &&
	        (child_result[i].listwt[child_result_offset[i]].list 
							== current_doc)) {
		if (op_ptr->op == AND_OP)
		    wt = 1.0-child_result[i].listwt[child_result_offset[i]].wt;
		else /* op is OR */
		    wt = child_result[i].listwt[child_result_offset[i]].wt;

#ifdef PNORMDEBUG
printf ("q_wt=%f, d_wt=%f, p=%f\n", q_wt[i], 
	child_result[i].listwt[child_result_offset[i]].wt, op_ptr->p);
#endif

		if (op_ptr->p != P_INFINITY) {
		    if ((double)UNDEF == 
		    (power=robust_pow(q_wt[i]*(double)wt, (double)op_ptr->p)))
			exit(4);
		    numerator += power;
		}
		else
		    numerator = MAX(numerator, wt*q_wt[i]);
		
		child_result_offset[i]++;
	    } /* if */
	    else {
	        if (NOTflag || op_ptr->op == AND_OP) {
		    if (op_ptr->p != P_INFINITY)
			numerator += disjoint_wt[i];
		    else
			numerator = MAX(numerator,disjoint_wt[i]);
	        }
	    } /* else */
	} /* end of for */

	/* enter this document in inv_result */
	if (inv_result->num_listwt == size) {
	    size +=16384;
	    if (NULL == (inv_result->listwt = (LISTWT *)
		realloc((char *)inv_result->listwt,size*sizeof(LISTWT)))) {
		fprintf( stderr, "pnorm: No more space\n");
		exit(1);
	    }
	}

	inv_result->listwt[inv_result->num_listwt].list = current_doc;
	inv_result->listwt[inv_result->num_listwt].wt = 
		eval_p_formula(op_ptr->p, op_ptr->op, numerator, denominator);

#ifdef PNORMDEBUG
printf ("result = %f\n", inv_result->listwt[inv_result->num_listwt].wt);
#endif

	inv_result->num_listwt++;
    } /* while */

    /* Deallocate the inverted lists of children */
    for (i=0; i<num_children; i++)
	(void) free((char *)child_result[i].listwt);

#ifdef PNORMDEBUG
{ int i;
    printf ("***********************************\n");
    for (i = 0; i < inv_result->num_listwt; i++) {
        (void) printf ("%ld\t%d\t%ld\t%f\n",
                        inv_result->id_num,
                        (int) 0,
                        inv_result->listwt[i].list,
                        inv_result->listwt[i].wt);
    }
    printf ("***********************************\n");
}
#endif

    /* compute the disjoint weight for this clause */
    if (NOTflag)
	*disjoint_val = eval_p_formula(op_ptr->p, op_ptr->op,
			disjoint_numerator, denominator);
    else 
	*disjoint_val = (double)0.0;

    return(1);

}  /* end of inv_eval_p */

