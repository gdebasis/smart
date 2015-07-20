#ifndef ADV_VECTORH
#define ADV_VECTORH

#include "eid.h"
#include "buf.h"
#include "adv_flags.h"

/* for indicating a span of ADV_VEC.query_text or a document */
typedef struct {
  long begin, end;
} SPAN_OFFSET;


/* annotation of query */
#define ADV_ANNOT_UNKNOWN	0
#define ADV_ANNOT_SUBTREE_DATA	1
#define ADV_ANNOT_LEAF_DATA	2	/* e.g., text at a leaf */
#define ADV_ANNOT_APP_SE_INFO	3
#define ADV_ANNOT_SE_APP_HELP	4
#define ADV_ANNOT_SE_APP_EXPL	5
#define ADV_ANNOT_SE_APP_MATCHES 6
#define ADV_ANNOT_COMMENT	7	/* never used??? */

typedef struct {
  long num_ret_docs;	/* number of documents to retrieve */
  float min_thresh;	/* must be exceeded to satisfy */
  float max_thresh;	/* must not be exceeded to satisfy */
  short flags;
} ANNOT_APP_SE;

typedef struct {
  EID docid;
  SPAN_OFFSET span_in_doc;
  float weight;
} ANNOT_SE_APP;

/* annotations about query */
typedef struct {
  long annotation_type;
  SPAN_OFFSET offset;	/* covered text, docid, or system-dependent data */
  union {
    ANNOT_APP_SE app_se;	/* if type == ADV_ANNOT_APP_SE_INFO */
    ANNOT_SE_APP se_app;	/* if type == ADV_ANNOT_SE_APP_MATCHES */
  } annot;
} Q_ANNOT;


/* annotations and attributes from query meant to match document annotations */
typedef struct {
  SPAN_OFFSET ann_type;		/* annotation type */
  SPAN_OFFSET attr_name;	/* attribute name */
  SPAN_OFFSET attr_type;	/* attribute type */
  SPAN_OFFSET attr_val;		/* attribute value */
} ANN_ATTR;

/* context requirements for documents */
typedef struct {
  long ann_attr_ptr;	/* pointer into ann_attr list */
  long distance;
  short flags;
} D_CONTEXT;


/* associated with an interior node of a query tree */
typedef struct {
  int op;		/* type of operator (AND, OR, ...) */
  int hr_type;		/* head relation type (if op wants one) */
  float p;		/* p value of op */
  D_CONTEXT context;	/* context term must be found in */
} OP_NODE;

/* associated with a leaf node of a query tree, describing what needs to be
 * matched in document.  at least one of ctype/concept or ann_attr must be
 * set. */
typedef struct {
  long ctype;		/* ctype/concept are UNDEF if not used */
  long concept;

  long ann_attr_ptr;	/* pointer into ann_attr list */
  long num_ann_attrs;	/* how many ann_attrs starting at
			   ADV_VEC.ann_attr_list[ann_attr_ptr]
			   are valid for this node */
} LEAF_NODE;


typedef struct {
  long info;		/* pointer into op_node or leaf_node list */
  float weight;
  long q_annotation;    /* pointer into annotation list */
  long num_annotations; /* how many annotations starting at
                           ADV_VEC.annotation_list[q_annotation]
                           are valid for this node
			   (all nodes get one SUBTREE_DATA annotation,
			   of the text below them) */
  long child;		/* pointer to leftmost child */
  long sibling;		/* pointer to next sibling to the right */
} TREE_NODE;


typedef struct {
  /* document collection indexes to search */
  long collection;	/* pointer into collection_texts/collection_list */
  long num_colls;	/* how many collections are valid for this infoneed */

  /* restrict set */
  long restrict_term_node;	/* pointer to TREE_NODE */
  long doc_ptr;		/* pointer into docid_texts/docid_list */
  long num_docs;	/* how many docids are valid for this infoneed */

  long head_node;		/* pointer to TREE_NODE heading this infoneed */
  float weight;			/* weight for this infoneed */
  long q_annotation;    /* pointer into annotation list */
  long num_annotations; /* how many annotations starting at
                           ADV_VEC.annotation_list[q_annotation]
                           are valid for this infoneed
			   (expected to be exactly one, giving SUBTREE_DATA) */
} INFO_NEED;


typedef struct {
  EID id_num;			/* query id */
  SM_BUF query_text;		/* text of the query */
  short flags;
  SPAN_OFFSET mergetype;	/* "engine_choice" or system-dependent */
  SPAN_OFFSET mergedata;	/* system-dependent */

  Q_ANNOT *annotation_list;	/* global annotation pool for query */
  long num_annotations;		/* number of entries in annotation_list */

  ANN_ATTR *ann_attr_list;	/* global ann_attr pool for query */
  long num_ann_attrs;		/* number of entries in ann_attr_list */

  OP_NODE *op_node_list;	/* ptr to op_node tuples */
  long num_op_nodes;		/* number of operator nodes in tree */

  LEAF_NODE *leaf_node_list;	/* ptr to leaf_node tuples */
  long num_leaf_nodes;		/* number of leaf_node tuples in query */

  TREE_NODE *tree;		/* tree defining structure of query */
  long num_nodes;		/* number of tree nodes in query */

  INFO_NEED *info_needs;	/* list of infoneeds for query */
  long num_infoneeds;		/* number of infoneeds */

  /* parallel arrays of external and internal representations of document
   * collection identifiers for infoneeds to search */
  SPAN_OFFSET *collection_texts;
  short *collection_list;
  long num_collections;

  /* parallel arrays of external and internal representations of document
   * identifiers for infoneed restrict sets */
  SPAN_OFFSET *docid_texts;
  EID *docid_list;
  long num_docids;

  long q_annotation;    	/* pointer into annotation list */
  long num_q_annotations; 	/* how many annotations starting at
				   ADV_VEC.annotation_list[q_annotation]
				   are valid for entire query */
} ADV_VEC;

#define REL_ADV_VEC 17
#endif /* ADV_VECTORH */



/* these, or something like them, should eventually be in feedback.h */
/* a feedback module will do something like
 *	(adv_vec, fdbks, num_fdbks, infoneed #) -> adv_vec
 */
typedef struct {
  EID docid;
  SPAN_OFFSET span_in_doc;	/* UNDEF if unspecified */
} DOC_FDBK;

typedef struct {
  char relevant;
  char is_doc;
  union {
    DOC_FDBK doc_fdbk;
    SM_BUF txt_fdbk;	/* pointers into ADV_VEC.query_text */
  } fdbk;
} ADV_FDBK;
