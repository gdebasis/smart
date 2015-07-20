#include "common.h"
#include "param.h"
#include "functions.h"
#include "buf.h"
#include "proc.h"
#include "vector.h"
#include "advq_op.h"
#include "infoneed_parse.h"

/* some common helper functions for the op routines */

/* set ifn_node according to kak in ifn_query at pos.
 *
 * this is more closely related to the particular language we're parsing
 * than we would like, but the struct fields are fixed for whatever ops
 * there might be, so this routine might as well be a superset of
 * characteristics for all ops and all known languages.
 */
static int
infoneed_parse_kak(ifn_query, pos, kak, ifn_node)
IFN_QUERY *ifn_query;
int *pos;
char *kak;
IFN_TREE_NODE *ifn_node;
{
    char c, *value_str;
    int retval, novalue, numeric, oldpos;
    SM_BUF *buf;

    /* skip kak and following whitespace in query */
    *pos += strlen(kak);

    c = ifn_query->query_text[*pos];
    while (*pos < ifn_query->end_pos && whitespace(c)) {
	(*pos)++;
	c = ifn_query->query_text[*pos];
    }

    /* booleans */
    novalue = TRUE;
    if (!strcmp(kak, "OUTPUT_QUERY"))
	ifn_node->flags |= ADV_FLAGS_OUTPUT_QUERY;
    else if (!strcmp(kak, "NO_OUTPUT_QUERY"))
	ifn_node->flags &= ~ADV_FLAGS_OUTPUT_QUERY;
    else if (!strcmp(kak, "OUTPUT_DOCS"))
	ifn_node->flags |= ADV_FLAGS_GET_DOCS;
    else if (!strcmp(kak, "NO_OUTPUT_DOCS"))
	ifn_node->flags &= ~ADV_FLAGS_GET_DOCS;
    else if (!strcmp(kak, "REFORMULATION"))
	ifn_node->flags |= ADV_FLAGS_REFORM;
    else if (!strcmp(kak, "NOREFORMULATION"))
	ifn_node->flags &= ~ADV_FLAGS_REFORM;
    else if (!strcmp(kak, "EXPANSION"))
	ifn_node->flags |= ADV_FLAGS_EXPAND;
    else if (!strcmp(kak, "NOEXPANSION"))
	ifn_node->flags &= ~ADV_FLAGS_EXPAND;
    else if (!strcmp(kak, "STEMMING"))
	ifn_node->flags |= ADV_FLAGS_STEM;
    else if (!strcmp(kak, "NOSTEMMING"))
	ifn_node->flags &= ~ADV_FLAGS_STEM;
    else if (!strcmp(kak, "HELP"))
	ifn_node->flags |= ADV_FLAGS_HELP;
    else if (!strcmp(kak, "NOHELP"))
	ifn_node->flags &= ~ADV_FLAGS_HELP;
    else if (!strcmp(kak, "EXPL"))
	ifn_node->flags |= ADV_FLAGS_EXPLAN;
    else if (!strcmp(kak, "NOEXPL"))
	ifn_node->flags &= ~ADV_FLAGS_EXPLAN;
    else if (!strcmp(kak, "MATCHES"))
	ifn_node->flags |= ADV_FLAGS_MATCHES;
    else if (!strcmp(kak, "NOMATCHES"))
	ifn_node->flags &= ~ADV_FLAGS_MATCHES;
    else if (!strcmp(kak, "DOCFREQ"))
	ifn_node->flags |= ADV_FLAGS_DOCFREQ;
    else if (!strcmp(kak, "NODOCFREQ"))
	ifn_node->flags &= ~ADV_FLAGS_DOCFREQ;
    else if (!strcmp(kak, "REL"))
	ifn_node->flags |= ADV_FLAGS_RELEVANT;
    else if (!strcmp(kak, "NONREL"))
	ifn_node->flags &= ~ADV_FLAGS_RELEVANT;
    else if (!strcmp(kak, "ORDERED"))
	ifn_node->flags |= ADV_FLAGS_ORDERED;
    else if (!strcmp(kak, "UNORDERED"))
	ifn_node->flags &= ~ADV_FLAGS_ORDERED;
    else if (!strcmp(kak, "EXACT"))
	ifn_node->weight = 1.0;
    else
	novalue = FALSE;
    
    if (c == '=') {	/* has attached value */
	/* skip up to value */
	(*pos)++;
	c = ifn_query->query_text[*pos];
	while (*pos < ifn_query->end_pos && whitespace(c)) {
	    (*pos)++;
	    c = ifn_query->query_text[*pos];
	}

	if (novalue) {	/* boolean kak with unwanted value */
	    /* skip value */
	    while (*pos < ifn_query->end_pos && !whitespace(c) && c != '>') {
		(*pos)++;
		c = ifn_query->query_text[*pos];
	    }
	    return(UNDEF);
	}
    } else {
	if (novalue)	/* boolean kak properly without value */
	    return(0);
	else	/* either unknown boolean kak, or value kak without value */
	    return(UNDEF);
    }

    /* ints */
    numeric = TRUE;
    value_str = &ifn_query->query_text[*pos];
    retval = 0;		/* no errors yet */

    if (!strcmp(kak, "HR_TYPE")) {
	if (1 > sscanf(value_str, "%ld", &ifn_node->hr_type))
	    retval = UNDEF;
    } else if (!strcmp(kak, "NUMBER_TO_RETRIEVE")) {
	if (1 > sscanf(value_str, "%ld", &ifn_node->num_ret_docs))
	    retval = UNDEF;
    } else if (!strcmp(kak, "SPAN_START")) {
	if (1 > sscanf(value_str, "%ld", &ifn_node->span_start))
	    retval = UNDEF;
    } else if (!strcmp(kak, "SPAN_END")) {
	if (1 > sscanf(value_str, "%ld", &ifn_node->span_end))
	    retval = UNDEF;
    } else if (!strcmp(kak, "DISTANCE")) {
	if (1 > sscanf(value_str, "%ld", &ifn_node->distance))
	    retval = UNDEF;

    /* floats */
    } else if (!strcmp(kak, "FUZZY")) {
	if (1 > sscanf(value_str, "%f", &ifn_node->fuzziness))
	    retval = UNDEF;
    } else if (!strcmp(kak, "WEIGHT")) {
	if (1 > sscanf(value_str, "%f", &ifn_node->weight))
	    retval = UNDEF;
    } else if (!strcmp(kak, "MIN_THRESHOLD")) {
	if (1 > sscanf(value_str, "%f", &ifn_node->min_thresh))
	    retval = UNDEF;
    } else if (!strcmp(kak, "MAX_THRESHOLD")) {
	if (1 > sscanf(value_str, "%f", &ifn_node->max_thresh))
	    retval = UNDEF;
    } else
	numeric = FALSE;
    
    if (numeric) {
	/* skip attached value (with possible junk at end) */
	while (*pos < ifn_query->end_pos && !whitespace(c) && c != '>') {
	    (*pos)++;
	    c = ifn_query->query_text[*pos];
	}

	return retval;
    }
    

    /* strings */
    /* there used to be multiple strings, and may be again... */
    buf = (SM_BUF *)0;
    if (!strcmp(kak, "MERGETYPE"))
	buf = &ifn_node->mergetype;
    
    if (buf) {
	buf->buf = &ifn_query->query_text[*pos];
	oldpos = *pos;
    } else {
	retval = UNDEF;
    }

    /* skip attached value */
    while (*pos < ifn_query->end_pos && !whitespace(c) && c != '>') {
	(*pos)++;
	c = ifn_query->query_text[*pos];
    }

    if (buf)
	buf->end = *pos - oldpos;

    return retval;
}

/* set ifn_node according to any kaks characteristics found in ifn_query
 * at pos.  return UNDEF if other characteristics are found in ifn_query
 * (or on other parsing problems).
 */
int
infoneed_parse_kaks(ifn_query, pos, kaks, ifn_node)
IFN_QUERY *ifn_query;
int *pos;
char **kaks;
IFN_TREE_NODE *ifn_node;
{
    char c;
    int retval, tmprv, i;

    retval = 0;		/* no errors yet */

    while (*pos < ifn_query->end_pos) {
	c = ifn_query->query_text[*pos];
	if (c == '>') {
	    (*pos)++;
	    break;
	} else if whitespace(c) {
	    (*pos)++;
	} else if (isalpha(c)) {
	    i = 0;
	    while (kaks[i]) {
		if (same_tag(&ifn_query->query_text[*pos], kaks[i]))
		    break;
		i++;
	    }

	    if (kaks[i]) {
		tmprv = infoneed_parse_kak(ifn_query, pos, kaks[i], ifn_node);
		if (UNDEF == tmprv)
		    retval = UNDEF;
	    } else {
		/* not an expected kak */
		retval = UNDEF;

		/* skip kak */
		while (*pos < ifn_query->end_pos && !sgml_breakchar(c)) {
		    (*pos)++;
		    c = ifn_query->query_text[*pos];
		}
		while (*pos < ifn_query->end_pos && whitespace(c)) {
		    (*pos)++;
		    c = ifn_query->query_text[*pos];
		}
		if (c == '=') {		/* skip attached value */
		    (*pos)++;
		    c = ifn_query->query_text[*pos];
		    while (*pos < ifn_query->end_pos && whitespace(c)) {
			(*pos)++;
			c = ifn_query->query_text[*pos];
		    }
		    while (*pos < ifn_query->end_pos &&
					!whitespace(c) && c != '>') {
			(*pos)++;
			c = ifn_query->query_text[*pos];
		    }
		}
	    }
	} else {
	    /* weird syntax */
	    retval = UNDEF;
	    (*pos)++;
	}
    }

    return retval;
}


/* these two sets are common to many operators, but their children's
 * operator numbers may still vary in different instantiations,
 * so we have to generate the specialized form and store it elsewhere
 */
#if 0
/* if this set were ever actually used in the grammar, OPERATOR cases
 * would appear next to all the OPERAND ones below */
static IFN_CHILDREN operatorkids[] = {
	{ "INDEPENDENT", IFN_ONE_OF_LIST, UNDEF },
	{ "AND", IFN_ONE_OF_LIST, UNDEF },
	{ "AND-NOT", IFN_ONE_OF_LIST, UNDEF },
	{ "OR", IFN_ONE_OF_LIST, UNDEF },
	{ "HEAD_RELATION", IFN_ONE_OF_LIST, UNDEF },
	{ "OTHER_OPER", IFN_ONE_OF_LIST, UNDEF },
	{ "", IFN_NONE, UNDEF }
};
#endif

static IFN_CHILDREN operandkids[] = {
	{ "QUOTED_TEXT", IFN_ONE_OF_LIST, UNDEF },
	{ "FULL_TERM", IFN_ONE_OF_LIST, UNDEF },
	{ "INDEPENDENT", IFN_ONE_OF_LIST, UNDEF },
	{ "AND", IFN_ONE_OF_LIST, UNDEF },
	{ "AND-NOT", IFN_ONE_OF_LIST, UNDEF },
	{ "OR", IFN_ONE_OF_LIST, UNDEF },
	{ "HEAD_RELATION", IFN_ONE_OF_LIST, UNDEF },
	{ "OTHER_OPER", IFN_ONE_OF_LIST, UNDEF },
	{ "", IFN_NONE, UNDEF }
};

int
infoneed_specialize_kids(kids, op_table)
IFN_CHILDREN *kids;
ADVQ_OP *op_table;
{
    IFN_CHILDREN *kid;
    ADVQ_OP *op;
    int op_num;

    for (kid = kids; kid->child_kind != IFN_NONE; kid++) {
	if (same_tag(kid->child_text, "OPERAND")) {
	    kid->child_op = IFN_OPERAND_OP;
	    continue;
	}

	for (op = op_table, op_num = 0; op->op_name; op++, op_num++) {
	    if (same_tag(kid->child_text, op->op_name)) {
		kid->child_op = op_num;
		break;
	    }
	}

	if (!op->op_name)
	    return(UNDEF);
    }

    return(0);
}

int
infoneed_specialize_operandkids(kids, op_table)
IFN_CHILDREN **kids;
ADVQ_OP *op_table;
{
    *kids = (IFN_CHILDREN *) malloc((unsigned)(sizeof(operandkids)));
    if ((IFN_CHILDREN *)0 == *kids)
	return(UNDEF);
    (void) bcopy((char *)operandkids, (char *)(*kids),
				(int)sizeof(operandkids));
    if (UNDEF == infoneed_specialize_kids(*kids, op_table))
	return(UNDEF);
    
    return(0);
}

/* is (at least) one of kid_list in child and siblings? */
static int
has_kid_from_list(child, kid_list)
IFN_TREE_NODE *child;
IFN_CHILDREN *kid_list;
{
    IFN_CHILDREN *kid;

    while (child) {
	for (kid = kid_list; kid->child_kind != IFN_NONE; kid++) {
	    if (child->op == kid->child_op)
		return(TRUE);
	}
	child = child->sibling;
    }

    return(FALSE);
}

/* is child in kid_list? */
static int
kid_in_list(child, kid_list)
IFN_TREE_NODE *child;
IFN_CHILDREN *kid_list;
{
    IFN_CHILDREN *kid;

    for (kid = kid_list; kid->child_kind != IFN_NONE; kid++) {
	if (child->op == kid->child_op)
	    return(TRUE);
    }

    return(FALSE);
}

int
infoneed_check_kids(ifn_node, kids, operand_kids)
IFN_TREE_NODE *ifn_node;
IFN_CHILDREN *kids, *operand_kids;
{
    IFN_CHILDREN *kid;
    IFN_TREE_NODE *child;
    int found;

    /* are all required kids there? */
    for (kid = kids; kid->child_kind != IFN_NONE; kid++) {
	if (IFN_ONE_REQUIRED == kid->child_kind ||
					IFN_ONE_OR_MORE == kid->child_kind) {
	    child = ifn_node->child;

	    switch (kid->child_op) {
		case IFN_OPERAND_OP:
		    if (!has_kid_from_list(child, operand_kids))
			return(UNDEF);
		    break;
		default:
		    while (child) {
			if (child->op == kid->child_op)
			    break;
			child = child->sibling;
		    }
		    if (!child)
			return(UNDEF);
		    break;
	    }
	}
    }

    /* are all children there allowed? */
    child = ifn_node->child;
    while (child) {
	found = FALSE;
	for (kid = kids; kid->child_kind != IFN_NONE; kid++) {
	    switch (kid->child_op) {
		case IFN_OPERAND_OP:
		    if (kid_in_list(child, operand_kids))
			found = TRUE;
		    break;
		default:
		    if (kid->child_op == child->op)
			found = TRUE;
		    break;
	    }
	    if (found)
		break;
	}
	if (kid->child_kind == IFN_NONE)
	    return(UNDEF);
	child = child->sibling;
    }

    return(0);
}
