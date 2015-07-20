#ifndef HIERH
#define HIERH
/* a single node in the hierarchy. Sorted on node_num */
typedef struct {
  long node_num;                /* unique identifier of this node */
  long item_id;                 /* if node is a leaf, id of the item the  */
                                /*     node represents; else UNDEF        */
  float level;                  /* the similarity value of this node */
  long parent;                  /* node_num of parent of this node */
  long  num_desc_leaves;        /* # of leaves that are descendants of node */
  short  num_children;          /* number of children this node has */
  long *children;               /* ptr to array of children's node numbers */
} HIER;

#define HIER_ROOT 0             /* This should not be changed since  */
                                /* seek_hier(NULL) seeks to node 0 */
#endif /* HIERH */
