#ifndef TREEH
#define TREEH
#define INIT_SIZE    1024
#define TREE_NULL   -1
#define TREE_ERROR  -2

typedef int NODE;
typedef int TREE;

typedef struct {
	  NODE parent;
	  NODE child;
	  NODE sibl;
	  TREE tree;		       /* tree # if root; else TREE_NULL */
	  long desc;		       /* user defined descriptive info */
	} _SNODE_STRUCT;

typedef struct {
	  NODE root;		       /* node # of the root of this tree */
	  TREE rlink;		       /* tree structure's right neighbor */
	  TREE llink;		       /* tree structure's left neighbor */
	  long name;		       /* user defined identifier of tree */
	} _STREE_STRUCT;

typedef struct {
	  unsigned number_nodes;       /* # of nodes there's memory space for */
	  NODE free_node;	       /* first node not allocated */
	  NODE never_node;	       /* first node such that it & all nodes */
				       /*   with > addresses never allocated */
	  _SNODE_STRUCT *nodes;        /* pointer to memory for nodes */
	  unsigned number_trees;       /* # of trees there's memory space for */
	  unsigned num_active_trees;   /* # of trees currently in use */
	  TREE free_tree;	       /* first tree structure not allocated */
	  TREE never_tree;	       /* like never_node, but for trees */
	  TREE first_tree;	       /* first tree in tree structure list */
	  _STREE_STRUCT *trees;        /* pointer to memory for trees */
	  NODE *children;	       /* pointer to list of children of node
					    last "GetChildren" was done for */
	  unsigned max_children;       /* maximum # children */
	} FOREST;

#endif /* TREEH */
