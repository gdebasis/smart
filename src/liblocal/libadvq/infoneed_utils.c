#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libadvq/infoneed_index.c.c,v 11.2 1997/09/17 16:51:18 smart Exp $";
#endif

/* Copyright (c) 1997 - Janet Walz.

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/
/********************   PROCEDURE DESCRIPTION   ************************
 *0 Utility routines for infoneed trees
 *1 none
 *2 init_ifn(ifn_node)
 *3   IFN_TREE_NODE *ifn_node;
 *2 infoneed_add_child(ifn_node, ifn_prev_child, ifn_this_child, curr_pos)
 *3   IFN_TREE_NODE *ifn_node;
 *3   IFN_TREE_NODE **ifn_prev_child;
 *3   IFN_TREE_NODE **ifn_this_child;
 *3   int curr_pos;
 *2 free_ifn_children(ifn_node)
 *3   IFN_TREE_NODE *ifn_node;

 *7 init_ifn() initializes an IFN_TREE_NODE with default values.
 *7 No return value.
 *7
 *7 infoneed_add_child() creates a new child, initializes it, and links it
 *7 into a tree of IFN_TREE_NODEs.
 *7 1 is returned on normal completion, UNDEF if error.
 *7
 *7 free_ifn_children() recursively frees ifn_node and all its descendants.
 *7 No return value.
***********************************************************************/

#include "common.h"
#include "infoneed_parse.h"

void
init_ifn(ifn_node)
IFN_TREE_NODE *ifn_node;
{
    ifn_node->op = UNDEF;
    ifn_node->fuzziness = 1.0;
    ifn_node->weight = 1.0;

    /* don't know what most of the flags should default to, but need
     * something here
     */
    ifn_node->flags = ADV_FLAGS_GET_DOCS |
			ADV_FLAGS_REFORM | ADV_FLAGS_EXPAND | ADV_FLAGS_STEM;

    ifn_node->num_ret_docs = 10;
    ifn_node->hr_type = UNDEF;
    ifn_node->distance = UNDEF;
    ifn_node->min_thresh = 0.0;
    ifn_node->max_thresh = 1.0;

    ifn_node->mergetype.buf = (char *)0;
    ifn_node->mergetype.end = 0;
    ifn_node->span_start = UNDEF;
    ifn_node->span_end = UNDEF;

    ifn_node->text_start = 0;
    ifn_node->text_end = 0;
    ifn_node->child = (IFN_TREE_NODE *)0;
    ifn_node->sibling = (IFN_TREE_NODE *)0;
    ifn_node->data.buf = (char *)0;
    ifn_node->data.end = 0;
    ifn_node->convec.id_num.id = UNDEF;
    ifn_node->convec.num_ctype = 0;
    ifn_node->convec.num_conwt = 0;
    ifn_node->convec.ctype_len = (long *)0;
    ifn_node->convec.con_wtp = (CON_WT *)0;
}

int
infoneed_add_child(ifn_node, ifn_prev_child, ifn_this_child, curr_pos)
IFN_TREE_NODE *ifn_node;
IFN_TREE_NODE **ifn_prev_child;
IFN_TREE_NODE **ifn_this_child;
int curr_pos;
{
    IFN_TREE_NODE *ifn_new;

    ifn_new = (IFN_TREE_NODE *) malloc(sizeof(IFN_TREE_NODE));
    if ((IFN_TREE_NODE *)0 == ifn_new) {
	return(UNDEF);
    } else {
	*ifn_prev_child = *ifn_this_child;
	*ifn_this_child = ifn_new;
	if (*ifn_prev_child)
	    (*ifn_prev_child)->sibling = ifn_new;
	else
	    ifn_node->child = ifn_new;

	init_ifn(ifn_new);
	ifn_new->text_start = curr_pos;
	ifn_new->text_end = UNDEF;
	return(1);
    }
}


void
free_ifn_children(ifn_node)
IFN_TREE_NODE *ifn_node;
{
    IFN_TREE_NODE *ifn_child, *ifn_tmp;

    for (ifn_child = ifn_node->child; ifn_child; ifn_child = ifn_tmp) {
	free_ifn_children(ifn_child);
	ifn_tmp = ifn_child->sibling;
	(void) free((char *)ifn_child->convec.ctype_len);
	(void) free((char *)ifn_child->convec.con_wtp);
	(void) free((char *)ifn_child);
    }
}
