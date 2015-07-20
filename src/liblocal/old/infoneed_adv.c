#include "common.h"
#include "param.h"
#include "spec.h"
#include "smart_error.h"
#include "functions.h"
#include "trace.h"
#include "inst.h"
#include "proc.h"
#include "vector.h"
#include "infoneed_parse.h"
#include "advq_op.h"
#include "adv_vector.h"


/* these should be a superset of all ops supported for parsing */
#define CONV_OP_DN2		1
#define CONV_OP_INFO_NEED	2
#define CONV_OP_QUOTED_TEXT	3
#define CONV_OP_FULL_TERM	4
#define CONV_OP_INDEPENDENT	5
#define CONV_OP_AND		6
#define CONV_OP_OR		7
#define CONV_OP_AND_NOT		8
#define CONV_OP_HEAD_RELATION	9
#define CONV_OP_OTHER_OPER	10
#define CONV_OP_OTHER_OPER_ARGS	11
#define CONV_OP_COMMENT		12
#define CONV_OP_ANN_ATTR	13
#define CONV_OP_ANN_TYPE	14
#define CONV_OP_ATTR_NAME	15
#define CONV_OP_ATTR_TYPE	16
#define CONV_OP_ATTR_VALUE	17
#define CONV_OP_APP_SE_INFO	18
#define CONV_OP_SE_APP_INFO	19
#define CONV_OP_SE_APP_HELP	20
#define CONV_OP_SE_APP_EXPL	21
#define CONV_OP_SE_APP_MATCHES	22
#define CONV_OP_MERGE_INFO	23
#define CONV_OP_WEIGHT		24
#define CONV_OP_DOC_COLLECTION	25
#define CONV_OP_RESTRICT_SET	26
#define CONV_OP_DOCID		27
#define CONV_OP_FEEDBACK_INFO	28
#define CONV_OP_DOCID_REL	29
#define CONV_OP_TEXT_REL	30
#define CONV_OP_CONTEXT		31

typedef struct {
    char *conv_op_name;
    int conv_op_num;
} CONV_OP;

CONV_OP conv_ops[] = {
    { "DN2", CONV_OP_DN2 },
    { "INFO_NEED", CONV_OP_INFO_NEED },
    { "QUOTED_TEXT", CONV_OP_QUOTED_TEXT },
    { "FULL_TERM", CONV_OP_FULL_TERM },
    { "INDEPENDENT", CONV_OP_INDEPENDENT },
    { "AND", CONV_OP_AND },
    { "OR", CONV_OP_OR },
    { "AND-NOT", CONV_OP_AND_NOT },
    { "HEAD_RELATION", CONV_OP_HEAD_RELATION },
    { "OTHER_OPER", CONV_OP_OTHER_OPER },
    { "OTHER_OPER_ARGS", CONV_OP_OTHER_OPER_ARGS },
    { "COMMENT", CONV_OP_COMMENT },
    { "ANN_ATTR", CONV_OP_ANN_ATTR },
    { "ANN_TYPE", CONV_OP_ANN_TYPE },
    { "ATTR_NAME", CONV_OP_ATTR_NAME },
    { "ATTR_TYPE", CONV_OP_ATTR_TYPE },
    { "ATTR_VALUE", CONV_OP_ATTR_VALUE },
    { "APP_SE_INFO", CONV_OP_APP_SE_INFO },
    { "SE_APP_INFO", CONV_OP_SE_APP_INFO },
    { "SE_APP_HELP", CONV_OP_SE_APP_HELP },
    { "SE_APP_EXPL", CONV_OP_SE_APP_EXPL },
    { "SE_APP_MATCHES", CONV_OP_SE_APP_MATCHES },
    { "MERGE_INFO", CONV_OP_MERGE_INFO },
    { "WEIGHT", CONV_OP_WEIGHT },
    { "DOC_COLLECTION", CONV_OP_DOC_COLLECTION },
    { "RESTRICT_SET", CONV_OP_RESTRICT_SET },
    { "DOCID", CONV_OP_DOCID },
    { "FEEDBACK_INFO", CONV_OP_FEEDBACK_INFO },
    { "DOCID_REL", CONV_OP_DOCID_REL },
    { "TEXT_REL", CONV_OP_TEXT_REL },
    { "CONTEXT", CONV_OP_CONTEXT },
    { (char *)0, UNDEF }
};


static long num_ops;
static SPEC_PARAM spec_args[] = {
    { "infoneed.num_ops",	getspec_long,	(char *) &num_ops },
    TRACE_PARAM ("index.parse.trace")
};
static int num_spec_args = sizeof(spec_args) / sizeof(spec_args[0]);

static char *op_prefix;
static char *op_name;
static SPEC_PARAM_PREFIX op_spec_args[] = {
    { &op_prefix, "name",	getspec_string, (char *) &op_name },
};
static int num_op_spec_args = sizeof(op_spec_args) / sizeof(op_spec_args[0]);


/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    ADVQ_OP *op_table;		/* table of advq operators */
    int *conv_op_nums;		/* list of corresponding numbers to use
				 * in conversion */
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;


static void count_ifn_tree();
static int alloc_adv_vec();

static int fill_adv_annot(), fill_adv_ann_attr();
static int fill_adv_doc_coll(), fill_adv_docid();
static int fill_adv_breadth();

static int fill_subtree_annotation(), fill_leaf_annotation();
static int fill_op_node();
static int fill_leaf_tree_node(), fill_leaf_node();
static int fill_dn2(), fill_infoneed();
static int fill_adv_vec();

static int process_feedbacks();


/* counts of various global arrays for an ADV_VEC */
typedef struct {
    long num_annots;
    long num_ann_attrs;
    long num_ops;
    long num_leafs;
    long num_nodes;
    long num_infoneeds;
    long num_colls;
    long num_docs;
} ADV_COUNT;

int
init_ifn_to_advv(spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;
    long i;
    char prefix_string[PATH_LEN];
    ADVQ_OP *advq_op;
    CONV_OP *conv_op;
    int op_num;

    NEW_INST(new_inst);
    if (UNDEF == new_inst)
	return(UNDEF);
    
    ip = &info[new_inst];

    if (UNDEF == lookup_spec(spec, &spec_args[0], num_spec_args))
	return(UNDEF);
    
    PRINT_TRACE (2, print_string, "Trace: entering init_ifn_to_advv");

    ip->op_table = (ADVQ_OP *)
			malloc ((unsigned) ((num_ops+1) * sizeof(ADVQ_OP)));
    if ((ADVQ_OP *)0 == ip->op_table)
	return(UNDEF);

    ip->conv_op_nums = (int *)
			malloc ((unsigned) ((num_ops+1) * sizeof(int)));
    if ((int *)0 == ip->conv_op_nums)
	return(UNDEF);

    op_prefix = prefix_string;
    advq_op = ip->op_table;
    for (i = 0; i < num_ops; i++) {
	(void) sprintf(prefix_string, "infoneed.op.%ld.", i);
	if (UNDEF == lookup_spec_prefix(spec, &op_spec_args[0],
							num_op_spec_args))
	    return(UNDEF);
	advq_op->op_name = op_name;
	advq_op++;
    }
    advq_op->op_name = (char *)0;		/* sentinel */
    
    /* find conversions for ops */
    for (advq_op = ip->op_table, op_num = 0; advq_op->op_name;
				advq_op++, op_num++) {
	for (conv_op = conv_ops; conv_op->conv_op_name; conv_op++) {
	    if (same_tag(conv_op->conv_op_name, advq_op->op_name)) {
		ip->conv_op_nums[op_num] = conv_op->conv_op_num;
		break;
	    }
	}
	if (!conv_op->conv_op_name)
	    return(UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving init_ifn_to_advv");

    ip->valid_info = 1;

    return(new_inst);
}

/* The IFN_TREE_NODE tree allows multiple DN2s, from multiple sections of
 * the document.  The ADV_VEC tree allows only one.  So, since we don't
 * plan to use the capability of having more than one DN2 anytime soon
 * in any case, assume there's only one DN2 in the IFN_TREE_NODE tree,
 * and have the global SUBTREE_DATA annotation be the text of that DN2.
 */
int
ifn_to_advv(ifn_root, adv_vec, inst)
IFN_TREE_NODE *ifn_root;
ADV_VEC *adv_vec;
int inst;
{
    STATIC_INFO *ip;
    ADV_COUNT adv_count;
    long curr_infoneed;

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "ifn_to_advv");
	return(UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering ifn_to_advv");

#if 0
    adv_count.num_annots = 1;
#else
    adv_count.num_annots = 0;
#endif
    adv_count.num_ann_attrs = adv_count.num_ops =
	adv_count.num_leafs = adv_count.num_nodes = adv_count.num_infoneeds =
	adv_count.num_colls = adv_count.num_docs = 0;

    count_ifn_tree(ifn_root, &adv_count, ip->conv_op_nums);

    if (UNDEF == alloc_adv_vec(adv_vec, &adv_count))
	return(UNDEF);

    /* adv_vec->id_num and adv_vec->query_text set by caller */
    adv_vec->flags = 0;
    adv_vec->mergetype.begin = adv_vec->mergetype.end = UNDEF;
    adv_vec->mergedata.begin = adv_vec->mergedata.end = UNDEF;

#if 0
    /* the string of the root node is the only whole-query annotation */
    adv_vec->q_annotation = 0;
    adv_vec->num_q_annotations = 1;
    adv_vec->annotation_list[0].annotation_type = ADV_ANNOT_SUBTREE_DATA;
    adv_vec->annotation_list[0].offset.begin = 0;
    adv_vec->annotation_list[0].offset.end = adv_vec->query_text.end;
    adv_count.num_annots = 1;
#endif

    adv_count.num_annots = 0;
    adv_count.num_ann_attrs = adv_count.num_ops =
	adv_count.num_leafs = adv_count.num_nodes = adv_count.num_infoneeds =
	adv_count.num_colls = adv_count.num_docs = 0;

    if (UNDEF == fill_adv_vec(ifn_root, adv_vec, &adv_count, ip->conv_op_nums))
	return(UNDEF);

    PRINT_TRACE (6, print_string, "---------- Before feedback:");
    PRINT_TRACE (6, print_advvec, adv_vec);

    curr_infoneed = 0;
    if (UNDEF == process_feedbacks(ifn_root, adv_vec, &curr_infoneed,
							ip->conv_op_nums))
	return(UNDEF);

    PRINT_TRACE (4, print_string, "---------- After feedback:");
    PRINT_TRACE (4, print_advvec, adv_vec);

    PRINT_TRACE (2, print_string, "Trace: leaving ifn_to_advv");
    return(1);
}

int
close_ifn_to_advv(inst)
int inst;
{
    STATIC_INFO *ip;

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "close_ifn_to_advv");
	return(UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering close_ifn_to_advv");

    (void) free((char *)ip->op_table);
    (void) free((char *)ip->conv_op_nums);
    ip->valid_info = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving close_ifn_to_advv");
    return(0);
}

int
ext2int_docid(buf, docid)
SM_BUF *buf;
EID *docid;
{
    long tmpext;
    int retval;

    /* the external version of a docid is either just nnnn or nnnn.extended
     * for an extended docid.  the direct sscanf should be safe as a
     * (stronger than) nonnumeric character was necessary to terminate
     * the docid parsing within SGML.
     */
    retval = sscanf(buf->buf, "%ld.%ld", &docid->id, &tmpext);
    if (0 >= retval) {
	docid->id = 1;
	EXT_NONE(docid->ext);
	return(UNDEF);
    } else if (1 == retval) {
	EXT_NONE(docid->ext);
    } else {
	docid->ext.type = (tmpext >> 28) & 0x0000000f;
	docid->ext.num = (tmpext & 0x0fffffff);
    }
    return(1);
}

int
ext2int_coll(unused)
SM_BUF *unused;
{
    /* until we define what a collection id looks like, and the rest of
     * SMART knows what to do with one... */
    return(1);	/* XXX */
}

static void
count_ifn_tree(ifn_node, adv_count, conv_op_nums)
IFN_TREE_NODE *ifn_node;
ADV_COUNT *adv_count;
int *conv_op_nums;
{
    int opnum, child_opnum;
    IFN_TREE_NODE *ifn_child;

    opnum = ifn_node->op;
    if (opnum >= 0)	/* real operator from SGML */
	opnum = conv_op_nums[opnum];

    switch (opnum) {
	case IFN_NO_OP:
	case IFN_ROOT_OP:
	case IFN_OPERATOR_OP:	/* shouldn't occur here */
	case IFN_OPERAND_OP:	/* shouldn't occur here */
	case CONV_OP_OTHER_OPER_ARGS:
	case CONV_OP_ANN_TYPE:
	case CONV_OP_ATTR_NAME:
	case CONV_OP_ATTR_TYPE:
	case CONV_OP_ATTR_VALUE:
	case CONV_OP_SE_APP_INFO:
	case CONV_OP_MERGE_INFO:
	case CONV_OP_WEIGHT:
	case CONV_OP_RESTRICT_SET:
	case CONV_OP_FEEDBACK_INFO:
	case CONV_OP_DOCID_REL:
	case CONV_OP_TEXT_REL:
	case CONV_OP_CONTEXT:
	    break;

	case CONV_OP_DN2:
	case CONV_OP_COMMENT:
	case CONV_OP_APP_SE_INFO:
	case CONV_OP_SE_APP_HELP:
	case CONV_OP_SE_APP_EXPL:
	case CONV_OP_SE_APP_MATCHES:
	    adv_count->num_annots++;
	    break;

	case CONV_OP_INFO_NEED:
	    adv_count->num_annots++;
	    adv_count->num_infoneeds++;
	    break;

	case IFN_TEXT_OP:
	case CONV_OP_QUOTED_TEXT:
	    adv_count->num_annots++;
	    adv_count->num_leafs += MAX(ifn_node->convec.num_conwt, 1);
	    adv_count->num_nodes += MAX(ifn_node->convec.num_conwt, 1);
	    break;

	case CONV_OP_FULL_TERM:
	    adv_count->num_annots++;
	    ifn_child = ifn_node->child;
	    while ((IFN_TREE_NODE *)0 != ifn_child) {
		child_opnum = ifn_child->op;
		if (child_opnum >= 0)
		    child_opnum = conv_op_nums[child_opnum];

		if (CONV_OP_QUOTED_TEXT == child_opnum)
		    break;
		ifn_child = ifn_child->sibling;
	    }
	    if ((IFN_TREE_NODE *)0 == ifn_child) {
		/* If there were text with this term, that node would
		 * increment the counts as necessary. */
		adv_count->num_leafs++;
		adv_count->num_nodes++;
	    }
	    break;


	case CONV_OP_INDEPENDENT:
	case CONV_OP_AND:
	case CONV_OP_OR:
	case CONV_OP_AND_NOT:
	case CONV_OP_HEAD_RELATION:
	case CONV_OP_OTHER_OPER:
	    adv_count->num_annots++;
	    adv_count->num_ops++;
	    adv_count->num_nodes++;
	    break;

	case CONV_OP_ANN_ATTR:
	    adv_count->num_ann_attrs++;
	    break;

	case CONV_OP_DOC_COLLECTION:
	    adv_count->num_colls++;
	    break;

	case CONV_OP_DOCID:
	    adv_count->num_docs++;
	    break;
    }

    ifn_child = ifn_node->child;
    while ((IFN_TREE_NODE *)0 != ifn_child) {
	count_ifn_tree(ifn_child, adv_count, conv_op_nums);
	ifn_child = ifn_child->sibling;
    }
}

static int
alloc_adv_vec(adv_vec, adv_count)
ADV_VEC *adv_vec;
ADV_COUNT *adv_count;
{
    if (0 == adv_count->num_annots) {
	adv_vec->annotation_list = (Q_ANNOT *)0;
    } else {
	adv_vec->annotation_list = (Q_ANNOT *)
		malloc ((unsigned) (adv_count->num_annots * sizeof(Q_ANNOT)));
	if ((Q_ANNOT *)0 == adv_vec->annotation_list)
	    return(UNDEF);
    }
    adv_vec->num_annotations = adv_count->num_annots;
    
    if (0 == adv_count->num_ann_attrs) {
	adv_vec->ann_attr_list = (ANN_ATTR *)0;
    } else {
	adv_vec->ann_attr_list = (ANN_ATTR *)
	    malloc ((unsigned) (adv_count->num_ann_attrs * sizeof(ANN_ATTR)));
	if ((ANN_ATTR *)0 == adv_vec->ann_attr_list)
	    return(UNDEF);
    }
    adv_vec->num_ann_attrs = adv_count->num_ann_attrs;
    
    if (0 == adv_count->num_ops) {
	adv_vec->op_node_list = (OP_NODE *)0;
    } else {
	adv_vec->op_node_list = (OP_NODE *)
		malloc ((unsigned) (adv_count->num_ops * sizeof(OP_NODE)));
	if ((OP_NODE *)0 == adv_vec->op_node_list)
	    return(UNDEF);
    }
    adv_vec->num_op_nodes = adv_count->num_ops;

    if (0 == adv_count->num_leafs) {
	adv_vec->leaf_node_list = (LEAF_NODE *)0;
    } else {
	adv_vec->leaf_node_list = (LEAF_NODE *)
		malloc ((unsigned) (adv_count->num_leafs * sizeof(LEAF_NODE)));
	if ((LEAF_NODE *)0 == adv_vec->leaf_node_list)
	    return(UNDEF);
    }
    adv_vec->num_leaf_nodes = adv_count->num_leafs;

    if (0 == adv_count->num_nodes) {
	adv_vec->tree = (TREE_NODE *)0;
    } else {
	adv_vec->tree = (TREE_NODE *)
		malloc ((unsigned) (adv_count->num_nodes * sizeof(TREE_NODE)));
	if ((TREE_NODE *)0 == adv_vec->tree)
	    return(UNDEF);
    }
    adv_vec->num_nodes = adv_count->num_nodes;

    if (0 == adv_count->num_infoneeds) {
	adv_vec->info_needs = (INFO_NEED *)0;
    } else {
	adv_vec->info_needs = (INFO_NEED *)
	    malloc ((unsigned) (adv_count->num_infoneeds * sizeof(INFO_NEED)));
	if ((INFO_NEED *)0 == adv_vec->info_needs)
	    return(UNDEF);
    }
    adv_vec->num_infoneeds = adv_count->num_infoneeds;

    if (0 == adv_count->num_colls) {
	adv_vec->collection_texts = (SPAN_OFFSET *)0;
	adv_vec->collection_list = (short *)0;
    } else {
	adv_vec->collection_texts = (SPAN_OFFSET *)
	    malloc ((unsigned) (adv_count->num_colls * sizeof(SPAN_OFFSET)));
	if ((SPAN_OFFSET *)0 == adv_vec->collection_texts)
	    return(UNDEF);
	adv_vec->collection_list = (short *)
		malloc ((unsigned) (adv_count->num_colls * sizeof(short)));
	if ((short *)0 == adv_vec->collection_list)
	    return(UNDEF);
    }
    adv_vec->num_collections = adv_count->num_colls;

    if (0 == adv_count->num_docs) {
	adv_vec->docid_texts = (SPAN_OFFSET *)0;
	adv_vec->docid_list = (EID *)0;
    } else {
	adv_vec->docid_texts = (SPAN_OFFSET *)
		malloc ((unsigned) (adv_count->num_docs * sizeof(SPAN_OFFSET)));
	if ((SPAN_OFFSET *)0 == adv_vec->docid_texts)
	    return(UNDEF);
	adv_vec->docid_list = (EID *)
		malloc ((unsigned) (adv_count->num_docs * sizeof(EID)));
	if ((EID *)0 == adv_vec->docid_list)
	    return(UNDEF);
    }
    adv_vec->num_docids = adv_count->num_docs;

    return(1);
}

static int
fill_adv_annot(ifn_node, adv_vec, adv_count, opnum)
IFN_TREE_NODE *ifn_node;
ADV_VEC *adv_vec;
ADV_COUNT *adv_count;
int opnum;
{
    Q_ANNOT *annot;

    annot = &(adv_vec->annotation_list[adv_count->num_annots]);
    annot->offset.begin = ifn_node->data.buf - adv_vec->query_text.buf;
    annot->offset.end = annot->offset.begin + ifn_node->data.end - 1;

    switch (opnum) {
	case CONV_OP_APP_SE_INFO:
	    annot->annotation_type = ADV_ANNOT_APP_SE_INFO;
	    annot->annot.app_se.num_ret_docs = ifn_node->num_ret_docs;
	    annot->annot.app_se.min_thresh = ifn_node->min_thresh;
	    annot->annot.app_se.max_thresh = ifn_node->max_thresh;
	    annot->annot.app_se.flags = (ifn_node->flags & ADV_FLAGS_ANNOT);
	    break;
	case CONV_OP_SE_APP_HELP:
	    annot->annotation_type = ADV_ANNOT_SE_APP_HELP;
	    break;
	case CONV_OP_SE_APP_EXPL:
	    annot->annotation_type = ADV_ANNOT_SE_APP_EXPL;
	    break;
	case CONV_OP_SE_APP_MATCHES:
	    annot->annotation_type = ADV_ANNOT_SE_APP_MATCHES;
	    if (UNDEF == ext2int_docid(&ifn_node->data,
						&annot->annot.se_app.docid))
		return(UNDEF);
	    annot->annot.se_app.span_in_doc.begin = ifn_node->span_start;
	    annot->annot.se_app.span_in_doc.end = ifn_node->span_end;
	    annot->annot.se_app.weight = ifn_node->weight;
	    break;
	default:
	    return(UNDEF);
	    break;
    }
    adv_count->num_annots++;
    return(1);
}

/* ifn_node is ANN_ATTR node */
static int
fill_adv_ann_attr(ifn_node, adv_vec, adv_count, conv_op_nums)
IFN_TREE_NODE *ifn_node;
ADV_VEC *adv_vec;
ADV_COUNT *adv_count;
int *conv_op_nums;
{
    ANN_ATTR *ann_attr;
    IFN_TREE_NODE *ifn_child;
    int opnum_child;

    ann_attr = &(adv_vec->ann_attr_list[adv_count->num_ann_attrs]);
    ann_attr->ann_type.begin = ann_attr->ann_type.end = UNDEF;
    ann_attr->attr_name.begin = ann_attr->attr_name.end = UNDEF;
    ann_attr->attr_type.begin = ann_attr->attr_type.end = UNDEF;
    ann_attr->attr_val.begin = ann_attr->attr_val.end = UNDEF;

    ifn_child = ifn_node->child;
    while ((IFN_TREE_NODE *)0 != ifn_child) {
	long abegin, aend;

	abegin = ifn_child->data.buf - adv_vec->query_text.buf;
	aend = abegin + ifn_child->data.end - 1;

	opnum_child = ifn_child->op;
	if (opnum_child >= 0)
	    opnum_child = conv_op_nums[opnum_child];

	switch (opnum_child) {
	case CONV_OP_ANN_TYPE:
	    ann_attr->ann_type.begin = abegin;
	    ann_attr->ann_type.end = aend;
	    break;
	case CONV_OP_ATTR_NAME:
	    ann_attr->attr_name.begin = abegin;
	    ann_attr->attr_name.end = aend;
	    break;
	case CONV_OP_ATTR_TYPE:
	    ann_attr->attr_type.begin = abegin;
	    ann_attr->attr_type.end = aend;
	    break;
	case CONV_OP_ATTR_VALUE:
	    ann_attr->attr_val.begin = abegin;
	    ann_attr->attr_val.end = aend;
	    break;
	default:
	    return(UNDEF);
	    break;
	}
	ifn_child = ifn_child->sibling;
    }
    adv_count->num_ann_attrs++;
    return(1);
}

static int
fill_adv_doc_coll(ifn_node, adv_vec, adv_count)
IFN_TREE_NODE *ifn_node;
ADV_VEC *adv_vec;
ADV_COUNT *adv_count;
{
    long cbegin, cend, colls;

    cbegin = ifn_node->data.buf - adv_vec->query_text.buf;
    cend = cbegin + ifn_node->data.end - 1;
    colls = adv_count->num_colls;

    adv_vec->collection_texts[colls].begin = cbegin;
    adv_vec->collection_texts[colls].end = cend;
    adv_vec->collection_list[colls] = ext2int_coll(&ifn_node->data);
    if (UNDEF == adv_vec->collection_list[colls])
	return(UNDEF);

    adv_count->num_colls++;
    return(1);
}

static int
fill_adv_docid(ifn_node, adv_vec, adv_count)
IFN_TREE_NODE *ifn_node;
ADV_VEC *adv_vec;
ADV_COUNT *adv_count;
{
    long dbegin, dend, docs;

    dbegin = ifn_node->data.buf - adv_vec->query_text.buf;
    dend = dbegin + ifn_node->data.end - 1;
    docs = adv_count->num_docs;

    adv_vec->docid_texts[docs].begin = dbegin;
    adv_vec->docid_texts[docs].end = dend;
    if (UNDEF == ext2int_docid(&ifn_node->data, &(adv_vec->docid_list[docs])))
	    return(UNDEF);
    
    adv_count->num_docs++;
    return(1);
}

/* some things, like annotations on a node, appear in various (grand)children
 * of an ifn_node, but need to be contiguous (and applied to the higher
 * node) in the adv_vec.  starts examining children of ifn_node.
 */
static int
fill_adv_breadth(ifn_node, adv_vec, adv_count, old_counts, conv_op_nums)
IFN_TREE_NODE *ifn_node;
ADV_VEC *adv_vec;
ADV_COUNT *adv_count, *old_counts;
int *conv_op_nums;
{
    IFN_TREE_NODE *ifn_child, *ifn_grandchild;
    int opnum_child, opnum_grandchild;

    ifn_child = ifn_node->child;
    while ((IFN_TREE_NODE *)0 != ifn_child) {
	opnum_child = ifn_child->op;
	if (opnum_child >= 0)
	    opnum_child = conv_op_nums[opnum_child];
	
	switch (opnum_child) {
	case CONV_OP_APP_SE_INFO:
	    if (UNDEF == fill_adv_annot(ifn_child, adv_vec, adv_count,
								opnum_child))
		return(UNDEF);
	    break;
	case CONV_OP_SE_APP_INFO:
	    ifn_grandchild = ifn_child->child;
	    while ((IFN_TREE_NODE *)0 != ifn_grandchild) {
		opnum_grandchild = ifn_grandchild->op;
		if (opnum_grandchild >= 0)
		    opnum_grandchild = conv_op_nums[opnum_grandchild];

		if (UNDEF == fill_adv_annot(ifn_grandchild, adv_vec,
						adv_count, opnum_grandchild))
		    return(UNDEF);
		ifn_grandchild = ifn_grandchild->sibling;
	    }
	    break;
	case CONV_OP_ANN_ATTR:
	    if (UNDEF == fill_adv_ann_attr(ifn_child, adv_vec, adv_count,
					conv_op_nums))
		return(UNDEF);
	    break;
	case CONV_OP_DOC_COLLECTION:
	    if (UNDEF == fill_adv_doc_coll(ifn_child, adv_vec, adv_count))
		return(UNDEF);
	    break;
	case CONV_OP_RESTRICT_SET:
	    ifn_grandchild = ifn_child->child;
	    while ((IFN_TREE_NODE *)0 != ifn_grandchild) {
		opnum_grandchild = ifn_grandchild->op;
		if (opnum_grandchild >= 0)
		    opnum_grandchild = conv_op_nums[opnum_grandchild];

		switch (opnum_grandchild) {
		    case CONV_OP_DOCID:
			if (UNDEF == fill_adv_docid(ifn_grandchild, adv_vec,
						    adv_count))
			    return(UNDEF);
			break;
		    case CONV_OP_FULL_TERM:
			if (UNDEF == fill_leaf_node(ifn_grandchild, adv_vec,
					adv_count, old_counts, conv_op_nums,
					opnum_grandchild))
			    return(UNDEF);
			break;
		}
		ifn_grandchild = ifn_grandchild->sibling;
	    }
	    break;
	}
	ifn_child = ifn_child->sibling;
    }

    return(1);
}

static int
fill_subtree_annotation(ifn_node, adv_vec, adv_count)
IFN_TREE_NODE *ifn_node;
ADV_VEC *adv_vec;
ADV_COUNT *adv_count;
{
    Q_ANNOT *annot;

    annot = &(adv_vec->annotation_list[adv_count->num_annots]);
    annot->annotation_type = ADV_ANNOT_SUBTREE_DATA;
    annot->offset.begin = ifn_node->text_start;
    annot->offset.end = ifn_node->text_end;
    adv_count->num_annots++;
    return(1);
}

static int
fill_leaf_annotation(ifn_node, adv_vec, adv_count)
IFN_TREE_NODE *ifn_node;
ADV_VEC *adv_vec;
ADV_COUNT *adv_count;
{
    Q_ANNOT *annot;

    annot = &(adv_vec->annotation_list[adv_count->num_annots]);
    annot->annotation_type = ADV_ANNOT_LEAF_DATA;
    annot->offset.begin = ifn_node->data.buf - adv_vec->query_text.buf;
    annot->offset.end = annot->offset.begin + ifn_node->data.end - 1;
    adv_count->num_annots++;
    return(1);
}

static int
fill_op_node(ifn_node, adv_vec, adv_count, old_counts, conv_op_nums, opnum)
IFN_TREE_NODE *ifn_node;
ADV_VEC *adv_vec;
ADV_COUNT *adv_count, *old_counts;
int *conv_op_nums;
int opnum;
{
    TREE_NODE *node;
    OP_NODE *op;
    IFN_TREE_NODE *ifn_child;
    int child_opnum;

    /* fill tree node */
    node = &(adv_vec->tree[old_counts->num_nodes]);
    node->info = old_counts->num_ops;
    node->weight = ifn_node->weight;
    node->q_annotation = old_counts->num_annots;

    if (UNDEF == fill_subtree_annotation(ifn_node, adv_vec, adv_count))
	return(UNDEF);
    /* process other annotations */
    if (UNDEF == fill_adv_breadth(ifn_node, adv_vec, adv_count, old_counts,
								conv_op_nums))
	return(UNDEF);
    node->num_annotations = adv_count->num_annots - old_counts->num_annots;

    node->child = UNDEF;
    node->sibling = UNDEF;
    adv_count->num_nodes++;


    /* fill op node */
    op = &(adv_vec->op_node_list[old_counts->num_ops]);
    op->op = ifn_node->op;
    if (CONV_OP_HEAD_RELATION == opnum)
	op->hr_type = ifn_node->hr_type;
    else if (CONV_OP_OTHER_OPER == opnum)
	op->hr_type = 42;	/* XXX fn(CONV_OP_OTHER_OPER_ARGS) */
    else
	op->hr_type = UNDEF;
    op->p = ifn_node->fuzziness;

    op->context.ann_attr_ptr = UNDEF;
    op->context.distance = UNDEF;
    op->context.flags = 0;
    ifn_child = ifn_node->child;
    while ((IFN_TREE_NODE *)0 != ifn_child) {
	child_opnum = ifn_child->op;
	if (child_opnum >= 0)
	    child_opnum = conv_op_nums[child_opnum];

	if (CONV_OP_CONTEXT == child_opnum) {
	    op->context.ann_attr_ptr = adv_count->num_ann_attrs;
	    op->context.distance = ifn_child->distance;
	    op->context.flags = (ifn_child->flags & ADV_FLAGS_CONTEXT);
	    /* process CONTEXT's ANN_ATTR child */
	    if (UNDEF == fill_adv_breadth(ifn_child, adv_vec, adv_count,
					    old_counts, conv_op_nums))
		return(UNDEF);
	}
	ifn_child = ifn_child->sibling;
    }
    adv_count->num_ops++;
    return(1);
}

static int
fill_leaf_tree_node(adv_vec, adv_count, old_counts, weight)
ADV_VEC *adv_vec;
ADV_COUNT *adv_count, *old_counts;
float weight;
{
    TREE_NODE *node, *prev_node;

    node = &(adv_vec->tree[adv_count->num_nodes]);	/* not old_counts */
    node->info = adv_count->num_leafs;			/* not old_counts */
    node->weight = weight;
    node->q_annotation = old_counts->num_annots;
    node->num_annotations = adv_count->num_annots - old_counts->num_annots;
    node->child = UNDEF;
    node->sibling = UNDEF;
    if (adv_count->num_nodes != old_counts->num_nodes) {
	prev_node = node - 1;
	prev_node->sibling = adv_count->num_nodes;
    }
	
    adv_count->num_nodes++;
    return(1);
}

static int
fill_leaf_node(ifn_node, adv_vec, adv_count, old_counts, conv_op_nums, opnum)
IFN_TREE_NODE *ifn_node;
ADV_VEC *adv_vec;
ADV_COUNT *adv_count, *old_counts;
int *conv_op_nums;
int opnum;
{
    IFN_TREE_NODE *ifn_text;
    int child_opnum;
    long num_ann_attrs;
    LEAF_NODE *leaf;
    long i, j, k;
    float weight;

    if (CONV_OP_FULL_TERM == opnum) {  /* concepts with potential child */
	if (UNDEF == fill_subtree_annotation(ifn_node, adv_vec, adv_count))
	    return(UNDEF);

	/* process further annotations and ann_attrs */
	if (UNDEF == fill_adv_breadth(ifn_node, adv_vec, adv_count, old_counts,
					    conv_op_nums))
	    return(UNDEF);

	ifn_text = ifn_node->child;
	while ((IFN_TREE_NODE *)0 != ifn_text) {
	    child_opnum = ifn_text->op;
	    if (child_opnum >= 0)
		child_opnum = conv_op_nums[child_opnum];

	    if (CONV_OP_QUOTED_TEXT == child_opnum) {
		break;
	    }
	    ifn_text = ifn_text->sibling;
	}
    } else		/* concepts with this node */
	ifn_text = ifn_node;

    if ((IFN_TREE_NODE *)0 != ifn_text) {
	if (UNDEF == fill_leaf_annotation(ifn_text, adv_vec, adv_count))
	return(UNDEF);
    }

    num_ann_attrs = adv_count->num_ann_attrs - old_counts->num_ann_attrs;

    if (((IFN_TREE_NODE *)0 != ifn_text) && (0 != ifn_text->convec.num_conwt)) {
	k = 0;
	for (i = 0; i < ifn_text->convec.num_ctype; i++) {
	    for (j = 0; j < ifn_text->convec.ctype_len[i]; j++) {
		/* XXX is this a safe way to tell whether a FULL_TERM
		 * weight is trying to override the ones from the concepts?
		 * 1.0 is the default weight in init_ifn...
		 */
		if (ifn_node->weight != 1.0) 
		    weight = ifn_node->weight;
		else
		    weight = ifn_text->convec.con_wtp[k].wt;
		if (UNDEF == fill_leaf_tree_node(adv_vec, adv_count,
							old_counts, weight))
		    return(UNDEF);

		leaf = &(adv_vec->leaf_node_list[adv_count->num_leafs]);
		leaf->ctype = i;
		leaf->concept = ifn_text->convec.con_wtp[k].con;
		leaf->ann_attr_ptr = old_counts->num_ann_attrs;
		leaf->num_ann_attrs = num_ann_attrs;
		k++;
		adv_count->num_leafs++;
	    }
	}
    } else {
	if (UNDEF == fill_leaf_tree_node(adv_vec, adv_count, old_counts,
							ifn_node->weight))
	    return(UNDEF);
	leaf = &(adv_vec->leaf_node_list[adv_count->num_leafs]);
	leaf->ctype = leaf->concept = UNDEF;
	leaf->ann_attr_ptr = old_counts->num_ann_attrs;
	leaf->num_ann_attrs = num_ann_attrs;
	adv_count->num_leafs++;
    }

    return(1);
}

static int
fill_dn2(ifn_node, adv_vec, adv_count, old_counts, conv_op_nums)
IFN_TREE_NODE *ifn_node;
ADV_VEC *adv_vec;
ADV_COUNT *adv_count, *old_counts;
int *conv_op_nums;
{
    IFN_TREE_NODE *ifn_child;
    int child_opnum;

    if (UNDEF == fill_subtree_annotation(ifn_node, adv_vec, adv_count))
	return(UNDEF);
    /* add further annotations */
    if (UNDEF == fill_adv_breadth(ifn_node, adv_vec, adv_count, old_counts,
								conv_op_nums))
	return(UNDEF);
    adv_vec->q_annotation = 0;
    adv_vec->num_q_annotations = adv_count->num_annots;

    /* look for mergeinfo */
    ifn_child = ifn_node->child;
    while ((IFN_TREE_NODE *)0 != ifn_child) {
	child_opnum = ifn_child->op;
	if (child_opnum >= 0)
	    child_opnum = conv_op_nums[child_opnum];
	
	if (CONV_OP_MERGE_INFO == child_opnum) {
	    adv_vec->mergetype.begin = ifn_child->mergetype.buf -
						adv_vec->query_text.buf;
	    adv_vec->mergetype.end = adv_vec->mergetype.begin +
						ifn_child->mergetype.end - 1;
	    adv_vec->mergedata.begin = ifn_child->data.buf -
						adv_vec->query_text.buf;
	    adv_vec->mergedata.end = adv_vec->mergedata.begin +
						ifn_child->data.end - 1;

	    break;
	}
	ifn_child = ifn_child->sibling;
    }
    if ((IFN_TREE_NODE *)0 == ifn_child) {
	adv_vec->mergetype.begin = adv_vec->mergetype.end = UNDEF;
	adv_vec->mergedata.begin = adv_vec->mergedata.end = UNDEF;
    }

    return(1);
}

static int
fill_infoneed(ifn_node, adv_vec, adv_count, old_counts, conv_op_nums)
IFN_TREE_NODE *ifn_node;
ADV_VEC *adv_vec;
ADV_COUNT *adv_count, *old_counts;
int *conv_op_nums;
{
    INFO_NEED *info;
    ADV_COUNT tmp_counts = *old_counts;

    if (UNDEF == fill_subtree_annotation(ifn_node, adv_vec, adv_count))
	return(UNDEF);
    /* process collections and restrict sets */
    tmp_counts.num_annots++;  /* restrict term doesn't count infoneed annot */
    if (UNDEF == fill_adv_breadth(ifn_node, adv_vec, adv_count, &tmp_counts,
								conv_op_nums))
	return(UNDEF);

    info = &(adv_vec->info_needs[old_counts->num_infoneeds]);
    info->num_colls = adv_count->num_colls - old_counts->num_colls;
    if (info->num_colls > 0)
	info->collection = old_counts->num_colls;
    else
	info->collection = UNDEF;

    if (adv_count->num_nodes != old_counts->num_nodes)
	info->restrict_term_node = old_counts->num_nodes;
    else
	info->restrict_term_node = UNDEF;

    info->doc_ptr = old_counts->num_docs;
    if (info->num_docs > 0)
	info->num_docs = adv_count->num_docs - old_counts->num_docs;
    else
	info->num_docs = UNDEF;

    info->weight = ifn_node->weight;
    info->q_annotation = old_counts->num_annots;
    info->num_annotations = adv_count->num_annots - old_counts->num_annots;

    /* head_node will be filled in by recursion */
    adv_count->num_infoneeds++;
    return(1);
}

static int
fill_adv_vec(ifn_node, adv_vec, adv_count, conv_op_nums)
IFN_TREE_NODE *ifn_node;
ADV_VEC *adv_vec;
ADV_COUNT *adv_count;
int *conv_op_nums;
{
    int opnum;
    ADV_COUNT old_counts;
    int do_tree_node, do_recurse;
    int infoneed_starting;

    old_counts = *adv_count;
    do_tree_node = do_recurse = FALSE;
    infoneed_starting = UNDEF;

    opnum = ifn_node->op;
    if (opnum >= 0)	/* real operator from SGML */
	opnum = conv_op_nums[opnum];

    switch (opnum) {
	case IFN_NO_OP:
	case IFN_ROOT_OP:
	case IFN_OPERATOR_OP:	/* shouldn't occur here */
	case IFN_OPERAND_OP:	/* shouldn't occur here */
	    do_recurse = TRUE;
	    break;

	case CONV_OP_ANN_ATTR:
	case CONV_OP_ANN_TYPE:
	case CONV_OP_ATTR_NAME:
	case CONV_OP_ATTR_TYPE:
	case CONV_OP_ATTR_VALUE:
	case CONV_OP_APP_SE_INFO:
	case CONV_OP_SE_APP_INFO:
	case CONV_OP_SE_APP_HELP:
	case CONV_OP_SE_APP_EXPL:
	case CONV_OP_SE_APP_MATCHES:
	case CONV_OP_DOC_COLLECTION:
	case CONV_OP_DOCID:
	case CONV_OP_FEEDBACK_INFO:
	case CONV_OP_DOCID_REL:
	case CONV_OP_TEXT_REL:
	case CONV_OP_OTHER_OPER_ARGS:
	case CONV_OP_COMMENT:
	case CONV_OP_MERGE_INFO:
	case CONV_OP_WEIGHT:	/* never used */
	case CONV_OP_RESTRICT_SET:
	case CONV_OP_CONTEXT:	/* never used */
	    break;

	case CONV_OP_DN2:
	    if (UNDEF == fill_dn2(ifn_node, adv_vec, adv_count, &old_counts,
								conv_op_nums))
		return(UNDEF);
	    do_recurse = TRUE;
	    break;

	case CONV_OP_INFO_NEED:
	    if (UNDEF == fill_infoneed(ifn_node, adv_vec, adv_count,
						&old_counts, conv_op_nums))
		return(UNDEF);
	    infoneed_starting = old_counts.num_infoneeds;
	    do_recurse = TRUE;
	    break;

	case IFN_TEXT_OP:
	case CONV_OP_QUOTED_TEXT:
	case CONV_OP_FULL_TERM:
	    if (UNDEF == fill_leaf_node(ifn_node, adv_vec, adv_count,
					&old_counts, conv_op_nums, opnum))
		return(UNDEF);
	    /* do not recurse here, as FULL_TERM's potential QUOTED_TEXT
	     * was already processed into the same node */
	    do_tree_node = TRUE;
	    break;

	case CONV_OP_INDEPENDENT:
	case CONV_OP_AND:
	case CONV_OP_OR:
	case CONV_OP_AND_NOT:
	case CONV_OP_HEAD_RELATION:
	case CONV_OP_OTHER_OPER:
	    if (UNDEF == fill_op_node(ifn_node, adv_vec, adv_count,
					&old_counts, conv_op_nums, opnum))
		return(UNDEF);
	    do_tree_node = do_recurse = TRUE;
	    break;
    }

    if (do_recurse) {
	IFN_TREE_NODE *ifn_child;
	long node_this_child, node_prev_child;

	ifn_child = ifn_node->child;
	node_prev_child = UNDEF;
	while ((IFN_TREE_NODE *)0 != ifn_child) {
	    node_this_child = adv_count->num_nodes;
	    if (UNDEF == fill_adv_vec(ifn_child, adv_vec, adv_count,
							conv_op_nums))
		return(UNDEF);
	    if (node_this_child != adv_count->num_nodes) {
		if (node_prev_child == UNDEF) {
		    if (UNDEF != infoneed_starting) {
			adv_vec->info_needs[infoneed_starting].head_node =
							node_this_child;
			node_prev_child = node_this_child;
		    } else if (do_tree_node) {
			adv_vec->tree[old_counts.num_nodes].child =
							node_this_child;
			node_prev_child = node_this_child;
		    }
		} else {
		    /* may have added extra siblings for multiple concepts */
		    while (UNDEF != adv_vec->tree[node_prev_child].sibling) {
		    	node_prev_child = adv_vec->tree[node_prev_child].sibling;
		    }
		    adv_vec->tree[node_prev_child].sibling = node_this_child;
		    node_prev_child = node_this_child;
		}
	    }
	    ifn_child = ifn_child->sibling;
	}
    }
    return(1);
}


/* we have to wait for an entire valid adv_vec to be constructed before
 * doing anything with feedback.  we only have to recurse over the kinds
 * of nodes found at the top of a tree, since feedbacks are always near
 * the top.
 */
static int
process_feedbacks(ifn_node, adv_vec, curr_infoneed, conv_op_nums)
IFN_TREE_NODE *ifn_node;
ADV_VEC *adv_vec;
long *curr_infoneed;
int *conv_op_nums;
{
    int opnum, child_opnum;
    IFN_TREE_NODE *ifn_child;
    int num_fdbks;
    ADV_FDBK *fdbks;

    opnum = ifn_node->op;
    if (opnum >= 0)
	opnum = conv_op_nums[opnum];

    switch (opnum) {
	case IFN_NO_OP:
	case IFN_ROOT_OP:
	case CONV_OP_DN2:
	    ifn_child = ifn_node->child;
	    while ((IFN_TREE_NODE *)0 != ifn_child) {
		if (UNDEF == process_feedbacks(ifn_child, adv_vec,
						curr_infoneed, conv_op_nums))
		    return(UNDEF);
		ifn_child = ifn_child->sibling;
	    }
	    break;
	case CONV_OP_INFO_NEED:
	    ifn_child = ifn_node->child;
	    while ((IFN_TREE_NODE *)0 != ifn_child) {
		if (UNDEF == process_feedbacks(ifn_child, adv_vec,
						curr_infoneed, conv_op_nums))
		    return(UNDEF);
		ifn_child = ifn_child->sibling;
	    }
	    (*curr_infoneed)++;
	    break;
	case CONV_OP_FEEDBACK_INFO:
	    ifn_child = ifn_node->child;
	    num_fdbks = 0;
	    while ((IFN_TREE_NODE *)0 != ifn_child) {
		num_fdbks++;
		ifn_child = ifn_child->sibling;
	    }

	    if (0 == num_fdbks) break;	/* makes no sense, but ... */

	    fdbks = (ADV_FDBK *)
			malloc((unsigned)(num_fdbks * sizeof(ADV_FDBK)));
	    if ((ADV_FDBK *)0 == fdbks)
		return(UNDEF);

	    ifn_child = ifn_node->child;
	    num_fdbks = 0;
	    while ((IFN_TREE_NODE *)0 != ifn_child) {
		child_opnum = ifn_child->op;
		if (child_opnum >= 0)
		    child_opnum = conv_op_nums[child_opnum];

		switch (child_opnum) {
		case CONV_OP_TEXT_REL:
		    fdbks[num_fdbks].is_doc = FALSE;
		    fdbks[num_fdbks].fdbk.txt_fdbk.buf = ifn_child->data.buf;
		    fdbks[num_fdbks].fdbk.txt_fdbk.end = ifn_child->data.end;
		    break;
		case CONV_OP_DOCID_REL:
		    fdbks[num_fdbks].is_doc = TRUE;
		    if (UNDEF == ext2int_docid(&ifn_child->data,
				&(fdbks[num_fdbks].fdbk.doc_fdbk.docid)))
			return(UNDEF);
		    fdbks[num_fdbks].fdbk.doc_fdbk.span_in_doc.begin =
						ifn_child->span_start;
		    fdbks[num_fdbks].fdbk.doc_fdbk.span_in_doc.end =
						ifn_child->span_end;
		    break;
		default:
		    return(UNDEF);
		    break;
		}

		fdbks[num_fdbks].relevant =
				((ifn_child->flags & ADV_FLAGS_RELEVANT) != 0);
		num_fdbks++;
		ifn_child = ifn_child->sibling;
	    }

#if 0
	    if (UNDEF == chris_feedback_procedure(adv_vec, fdbks, num_fdbks,
							*curr_infoneed))
		return(UNDEF);
#endif
	    (void) free((char *)fdbks);

	    break;
	default:
	    break;
    }

    return(1);
}
