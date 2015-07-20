#ifndef INFONEED_PARSEH
#define INFONEED_PARSEH

#include "adv_flags.h"
#include "buf.h"
#include "vector.h"

#define IFN_ONE_OPTIONAL	1
#define IFN_ONE_REQUIRED	2
#define IFN_ONE_OR_MORE		3
#define IFN_ZERO_OR_MORE	4
#define IFN_ONE_OF_LIST		5
#define IFN_NONE		6

/* for syntax checking */
typedef struct ifn_children {
    char *child_text;	/* text of child operator, or of generic ones below */
    int child_kind;	/* one of the above IFN_*s */
    int child_op;	/* op number corresponding to child_text */
} IFN_CHILDREN;

#define IFN_NO_OP	-1	/* for dummy subtree */
#define IFN_ROOT_OP	-2	/* top of tree */
#define IFN_TEXT_OP	-3	/* text leaves */
#define IFN_OPERATOR_OP	-4	/* syntax check on generic infoneed operator */
#define IFN_OPERAND_OP	-5	/* syntax check on generic infoneed operand */

typedef struct ifn_tree_node {
    int op;			/* type of operator (AND, OR, NOT, ...) */
    float fuzziness;		/* EXACT = 1, FUZZY = {0..1} */
    float weight;
    short flags;		/* various bitflags for various operators */

    /* fields used by various of the fancier operators */
    long num_ret_docs;		/* number of documents to retrieve */
    long hr_type;		/* head relation type */
    long distance;		/* context */

    float min_thresh;		/* must be exceeded to satisfy */
    float max_thresh;		/* must not be exceeded to satisfy */

    SM_BUF mergetype;		/* to merge multiple INFO_NEEDs */
    long span_start, span_end;

    /* fields used for tree structure itself */
    int text_start, text_end;	/* locations in query.text of text of interest
				 * to this subtree, including <> and </> */
    struct ifn_tree_node *child;	/* pointer to leftmost child */
    struct ifn_tree_node *sibling;	/* pointer to next sibling */
    SM_BUF data;		/* data or text strictly between <> and </>
				 * (when a leaf) */
    SM_VECTOR convec;		/* concepts associated with this node,
				 * if it's a text leaf */
} IFN_TREE_NODE;

typedef struct {
    char *query_text;		/* text of query */
    int start_pos, end_pos;	/* positions of text for this section */
    int pos;			/* current parsing location in text */
    int ifn_inst;		/* inst for main infoneed_parse stuff, as
				 * opposed to various op procedures, which we
				 * have to squirrel away to pass through the
				 * op procedures */
} IFN_QUERY;

typedef struct {
    IFN_TREE_NODE *ifn_node;	/* structure for debug printing */
    char *query_text;
} IFN_PRINT;

#define whitespace(c)		((c) == ' ' || (c) == '\t' || (c) == '\n')
#define sgml_breakchar(c)	(whitespace(c) || (c) == '=' || (c) == '>')

extern void init_ifn();			/* from infoneed_utils.c */
extern int infoneed_add_child();	/* from infoneed_utils.c */
extern void free_ifn_children();	/* from infoneed_utils.c */
extern int same_tag();			/* from infoneed_parse.c */
extern int infoneed_parse_data();	/* from infoneed_parse.c */
extern int infoneed_parse_args();	/* from infoneed_parse.c */
extern int infoneed_parse_kaks();	/* from info_helpers.c */
extern int infoneed_specialize_kids();	/* from info_helpers.c */
extern int infoneed_specialize_operatorkids();	/* from info_helpers.c */
extern int infoneed_specialize_operandkids();	/* from info_helpers.c */
extern int infoneed_check_kids();	/* from info_helpers.c */

#endif	/* INFONEED_PARSEH */
