#ifdef RCSID
static char rcsid[] = "$Header: make_vector.c,v 8.0 85/06/03 17:37:51 smart Exp $";
#endif

/* Copyright (c) 1984 - Gerard Salton, Chris Buckley, Ellen Voorhees */

#include <stdio.h>
#include "functions.h"
#include "common.h"
#include "param.h"
#include "rel_header.h"
#include "vector.h"
#include "pnorm_vector.h"
#include "pnorm_common.h"
#include "pnorm_indexing.h"
#include "spec.h"
#include "trace.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("index.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

extern NODE_INFO *node_info;
extern TREE_NODE *tree;
extern PNORM_VEC qvector;
extern int vec_index;
extern long no_ctypes;
extern SPEC *pnorm_coll_spec;

int
make_vector(no_nodes)
int no_nodes;				/* number of node_info nodes */
{
    PNORM_VEC	*vec_ptr;		/* pointer to query vector */
    CON_WT	cw_tuples[MAX_VEC];	/* concept-weight prs for this query */
    OP_P_WT	op_tuples[MAX_VEC];	/* operator tuples for this query */
    long 	ct_lens[MAX_NUM_CTYPES];/* lengths of ctpye subvectors */
    long	*ct_ptr;		/* index into ct_lens array */
    long	no_ctype_tuples;	/* # of cw_tuples for current ctype */
    long	num_tuples;		/* number of tuples */
    register CON_WT	*cw_ptr;	/* index into cw_tuples array */
    register OP_P_WT	*op_ptr;	/* index into operator tuples array */
    register NODE_INFO	*ni_ptr;	/* index into node_info array */

    int write_pnorm(), seek_pnorm();
    static int info_comp();		/* compares 2 node_info structs */

    if (UNDEF == lookup_spec (pnorm_coll_spec, &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering make_vector");

    if (no_nodes == 0)
        return(0);

    vec_ptr = &qvector;

    /* sort info nodes on ctype, type, concept, weight, p */
    qsort((char *)node_info,no_nodes,sizeof(NODE_INFO),info_comp);

    /* create operator tuples */
    num_tuples = 0;
    ni_ptr = node_info;
    op_ptr = &(op_tuples[0]);
    while (UNDEF == ni_ptr->ctype) {
        if (MAX_VEC <= num_tuples) {
          fprintf(stderr,"make_vector: too many operator tuples.\n");
          print_error("make_vector","Query ignored");
          return(UNDEF);
        }
        op_ptr->op = ni_ptr->type;
        op_ptr->p  = ni_ptr->p;
        op_ptr->wt = ni_ptr->wt;
        op_ptr++;
        (tree+ni_ptr->node)->info = num_tuples;
        num_tuples++;
        ni_ptr++;
    } /* while */
    vec_ptr->num_op_p_wt = num_tuples;
    vec_ptr->op_p_wtp = &(op_tuples[0]);

    /* make leaf cw_tuples; keep track of number of tuples for each ctype */
    for (ct_ptr= &(ct_lens[0]); ct_ptr< &(ct_lens[no_ctypes]); ct_ptr++) {
        *ct_ptr = 0;
    }
    num_tuples = 0;
    cw_ptr = &(cw_tuples[0]);
    no_ctype_tuples = 0;
    for ( ; ni_ptr<node_info+no_nodes; ni_ptr++) {
        if (MAX_NUM_CTYPES == ni_ptr->ctype)
          break;    /* reached 'discarded' nodes */
        if ( ni_ptr != node_info && (ni_ptr-1)->ctype != UNDEF &&
            (ni_ptr-1)->ctype != ni_ptr->ctype ) {
          ct_lens[(ni_ptr-1)->ctype] = no_ctype_tuples;
	  no_ctype_tuples = 0;
        }
        if (MAX_VEC <= num_tuples) {
          fprintf(stderr,"make_vector: too many con-wt pairs.\n");
          print_error("make_vector","Query ignored");
          return(UNDEF);
        }
	cw_ptr->con = ni_ptr->con;
	cw_ptr->wt = ni_ptr->wt;
        cw_ptr++;
        no_ctype_tuples++;
        (tree+ni_ptr->node)->info = num_tuples;
        num_tuples++;
    } /* for */
    if (0 == num_tuples)
        return(0);
    if ((ni_ptr-1) >= node_info && (ni_ptr-1)->ctype != UNDEF) {
	ct_lens[(ni_ptr-1)->ctype] = no_ctype_tuples;
    }
    vec_ptr->num_ctype = no_ctypes;
    vec_ptr->num_conwt = num_tuples;
    vec_ptr->ctype_len = &(ct_lens[0]);
    vec_ptr->con_wtp = &(cw_tuples[0]);

    if (0 != seek_pnorm(vec_index,vec_ptr) ||
        1 != write_pnorm(vec_index,vec_ptr)   ) {
        print_error("make_vector","quit");
        exit(1);    
    }

    PRINT_TRACE (2, print_string, "Trace: leaving make_vector");
    return(1);
}  /* make_vector */



static int
info_comp(a_ptr,b_ptr)		/* sort on ctype,type,concept,p,weight */
NODE_INFO *a_ptr,*b_ptr;
{

    if (a_ptr->ctype < b_ptr->ctype)
      return(-1);
    if (a_ptr->ctype > b_ptr->ctype)
      return(1);

    if (a_ptr->type < b_ptr->type)
      return(-1);
    if (a_ptr->type > b_ptr->type)
      return(1);

    if (a_ptr->con < b_ptr->con)
      return(-1);
    if (a_ptr->con > b_ptr->con)
      return(1);

    if (a_ptr->p < b_ptr->p)
      return(-1);
    if (a_ptr->p > b_ptr->p)
      return(1);

    if (a_ptr->wt < b_ptr->wt)
      return(-1);
    if (a_ptr->wt > b_ptr->wt)
      return(1);

    return(0);

} /* info_comp */
