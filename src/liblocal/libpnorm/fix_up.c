#ifdef RCSID
static char rcsid[] = "$Header: fix_up.c,v 8.0 85/06/03 17:37:49 smart Exp $";
#endif

/* Copyright (c) 1984 - Gerard Salton, Chris Buckley, Ellen Voorhees */

#include <stdio.h>
#include "functions.h"
#include "common.h"
#include "param.h"
#include "vector.h"
#include "pnorm_vector.h"
#include "pnorm_common.h"
#include "pnorm_indexing.h"
#include "spec.h"
#include "trace.h"

extern NODE_INFO *node_info;
extern TREE_NODE *tree;
extern unsigned num_tree_nodes;
extern PNORM_VEC qvector;
extern SPEC *pnorm_coll_spec;

static TREE_NODE *modified_tree;
static short num_modified_nodes;

static short fix_tree(), copy_tree();

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("index.trace")
    };
static int num_spec_args =
    sizeof (spec_args) / sizeof (spec_args[0]);


int
fix_up(root)
short	root;
{
    short new_root;

    if (UNDEF == lookup_spec (pnorm_coll_spec, &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering fix_up");

    if (NULL == (char *)(modified_tree = (TREE_NODE *)
        malloc(num_tree_nodes*sizeof(TREE_NODE)))) {
        print_error("fix_up", "Query ignored");
        return(UNDEF);
    }
    num_modified_nodes = 0;

    new_root = fix_tree(root);
    if (UNDEF == new_root) {
        return(0);
    }

    /* copy modified_tree back to tree, making sure root is node 0 */
    num_modified_nodes = 0;
    (void)copy_tree(new_root);

    qvector.num_nodes = (long)num_modified_nodes;
    qvector.tree = tree;  

    free((char *) modified_tree);

    PRINT_TRACE (2, print_string, "Trace: leaving fix_up");
    return(0);
} /* fix_up */


static short
fix_tree(root)
short	root;				/* number of root in orig tree */
{
    NODE_INFO *ni_ptr;                  /* ptr to node descriptors */
    register TREE_NODE *old_tree_ptr;   /* ptr into original tree structure */
    register TREE_NODE *new_tree_ptr;   /* ptr into modified tree structure */

    short child, new_child, good_child;
    short num_children;

    old_tree_ptr = tree + root;
    new_tree_ptr = modified_tree + num_modified_nodes;
    new_tree_ptr->sibling = UNDEF;
    new_tree_ptr->child = UNDEF;
    new_tree_ptr->info = old_tree_ptr->info;
    num_modified_nodes++;
    ni_ptr = node_info + old_tree_ptr->info;

    if (ni_ptr->type == UNDEF) {
        if (ni_ptr->con == UNDEF) {
            /* a concept that did not appear in the dictionary - remove node and
               set ctype in node_info so this descriptor will be discarded  */
            ni_ptr->ctype = MAX_NUM_CTYPES;
            num_modified_nodes--;
            return(UNDEF);
        }
        return(num_modified_nodes - 1);
    } /* outer if */
    else {
        num_children = 0;
        /* an operator node - first fix_up children */
        for (child = old_tree_ptr->child; child != UNDEF;
    	    child = (tree + child)->sibling) {
            new_child = fix_tree(child);
            if (new_child != UNDEF) {
                num_children++;
                good_child = new_child;
                (modified_tree+new_child)->sibling = new_tree_ptr->child;
                new_tree_ptr->child = new_child;
            }
        } /* for */
            
        /* make sure this subtree represents a legal clause */
        if (0 == num_children) {
            ni_ptr->ctype = MAX_NUM_CTYPES;
            num_modified_nodes--;
            return(UNDEF);
        }
        else if (num_children == 1 && ni_ptr->type != NOT_OP) {
            ni_ptr->ctype = MAX_NUM_CTYPES;
            return(good_child);
        }

        return((short)(new_tree_ptr - modified_tree));
    } /* else */

} /* fix_tree */


static short
copy_tree(root)
short	root;
{
    register TREE_NODE *tree_ptr;
    register TREE_NODE *modified_tree_ptr;
    short child, tree_child;

    modified_tree_ptr = modified_tree + root;
    tree_ptr = tree + num_modified_nodes;
    tree_ptr->info = modified_tree_ptr->info;
    (node_info+tree_ptr->info)->node = num_modified_nodes;
    tree_ptr->child = UNDEF;
    tree_ptr->sibling = UNDEF;
    num_modified_nodes++;

    for (child = modified_tree_ptr->child; UNDEF != child;
        child = (modified_tree+child)->sibling) {
        tree_child = copy_tree(child);
        (tree+tree_child)->sibling = tree_ptr->child;
        tree_ptr->child = tree_child;
    } /* for */

    return((short)(tree_ptr - tree));
} /* copy_tree */
