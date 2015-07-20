#include "common.h"
#include "param.h"
#include "smart_error.h"
#include "functions.h"
#include "buf.h"
#include "docindex.h"
#include "vector.h"
#include "adv_vector.h"

int
print_advvec(adv_vec, outbuf)
ADV_VEC *adv_vec;
SM_BUF *outbuf;
{
    char fmtbuf[80], linebuf[PATH_LEN];
    int i;
    SPAN_OFFSET *offset;
    EID *eid;

    /* print header stuff */
    (void) sprintf(linebuf, "ADV_VEC %ld.%u.%u    Flags: %.4x    Text:",
				adv_vec->id_num.id, adv_vec->id_num.ext.type,
				adv_vec->id_num.ext.num,
				adv_vec->flags);
    print_string(linebuf, outbuf);

    if (outbuf == NULL) {
	if (0 == fwrite(adv_vec->query_text.buf, adv_vec->query_text.end, 1,
						stdout)) {
	    (void) fprintf(stderr, "advvec %ld cannot be written - ignored\n",
						adv_vec->id_num.id);
	    return(UNDEF);
	}
    } else {
	if (UNDEF == add_buf(&adv_vec->query_text, outbuf)) {
	    (void) fprintf(stderr, "advvec %ld cannot be written - ignored\n",
						adv_vec->id_num.id);
	    return(UNDEF);
	}
    }
    (void) sprintf(linebuf, "\n\n");
    print_string(linebuf, outbuf);

    (void) sprintf(fmtbuf, "Mergetype: %%.%lds    Mergedata: %%.%lds",
			adv_vec->mergetype.end - adv_vec->mergetype.begin + 1,
			adv_vec->mergedata.end - adv_vec->mergedata.begin + 1);
    (void) sprintf(linebuf, fmtbuf,
			adv_vec->query_text.buf + adv_vec->mergetype.begin,
			adv_vec->query_text.buf + adv_vec->mergedata.begin);
    print_string(linebuf, outbuf);

    /* print annotations */
    (void) sprintf(linebuf, "\n%ld annotations starting at %ld for whole query",
			adv_vec->num_q_annotations, adv_vec->q_annotation);
    print_string(linebuf, outbuf);
    (void) sprintf(linebuf, "Annotations (%ld):", adv_vec->num_annotations);
    print_string(linebuf, outbuf);
    for (i = 0; i < adv_vec->num_annotations; i++) {
	Q_ANNOT *annot;
	ANNOT_SE_APP *se_app;

	annot = &(adv_vec->annotation_list[i]);
	switch (annot->annotation_type) {
	    case ADV_ANNOT_APP_SE_INFO:
		(void) sprintf(linebuf,
"%d:  APP_SE from %ld to %ld getting %ld docs between %f and %f with flags %.4x",
				i, annot->offset.begin, annot->offset.end,
				annot->annot.app_se.num_ret_docs,
				annot->annot.app_se.min_thresh,
				annot->annot.app_se.max_thresh,
				annot->annot.app_se.flags);
		break;
	    case ADV_ANNOT_SE_APP_MATCHES:
		se_app = &(annot->annot.se_app);
		(void) sprintf(linebuf,
"%d:  SE_APP from %ld to %ld for doc %ld.%u.%u from %ld to %ld with wt %f",
				i, annot->offset.begin, annot->offset.end,
				se_app->docid.id,
				se_app->docid.ext.type, se_app->docid.ext.num,
				se_app->span_in_doc.begin,
				se_app->span_in_doc.end,
				se_app->weight);
		break;
	    default:
		(void) sprintf(fmtbuf, "%%d:  type %%ld <%%.%lds>",
				annot->offset.end - annot->offset.begin + 1);
		(void) sprintf(linebuf, fmtbuf,
				i, annot->annotation_type,
				adv_vec->query_text.buf + annot->offset.begin);
		break;
	}
	print_string(linebuf, outbuf);
    }

    /* print ann_attrs */
    (void) sprintf(linebuf, "\nAnn_attrs (%ld):", adv_vec->num_ann_attrs);
    print_string(linebuf, outbuf);
    for (i = 0; i < adv_vec->num_ann_attrs; i++) {
	ANN_ATTR *ann_attr;

	ann_attr = &(adv_vec->ann_attr_list[i]);
	(void) sprintf(fmtbuf,
		"%%d:  type %%.%lds with attribute <%%.%lds,%%.%lds,%%.%lds>",
		ann_attr->ann_type.end - ann_attr->ann_type.begin + 1,
		ann_attr->attr_name.end - ann_attr->attr_name.begin + 1,
		ann_attr->attr_type.end - ann_attr->attr_type.begin + 1,
		ann_attr->attr_val.end - ann_attr->attr_val.begin + 1);
	(void) sprintf(linebuf, fmtbuf, i,
		adv_vec->query_text.buf + ann_attr->ann_type.begin,
		adv_vec->query_text.buf + ann_attr->attr_name.begin,
		adv_vec->query_text.buf + ann_attr->attr_type.begin,
		adv_vec->query_text.buf + ann_attr->attr_val.begin);
	print_string(linebuf, outbuf);
    }

    /* print op nodes */
    (void) sprintf(linebuf, "\nOp_nodes (%ld):", adv_vec->num_op_nodes);
    print_string(linebuf, outbuf);
    for (i = 0; i < adv_vec->num_op_nodes; i++) {
    	OP_NODE *op_node;

	op_node = &(adv_vec->op_node_list[i]);
	(void) sprintf(linebuf, "%d:  op %d  hr_type %d  p %f",
		i, op_node->op, op_node->hr_type, op_node->p);
	print_string(linebuf, outbuf);
	(void) sprintf(linebuf,
		"\tcontext  ann_attr_ptr %ld  distance %ld  flags %.4x",
		op_node->context.ann_attr_ptr, op_node->context.distance,
		op_node->context.flags);
	print_string(linebuf, outbuf);
    }

    /* print leaf nodes */
    (void) sprintf(linebuf, "\nLeaf_nodes (%ld):", adv_vec->num_leaf_nodes);
    print_string(linebuf, outbuf);
    for (i = 0; i < adv_vec->num_leaf_nodes; i++) {
    	LEAF_NODE *leaf_node;

	leaf_node = &(adv_vec->leaf_node_list[i]);
	(void) sprintf(linebuf,
		"%d:  ctype %ld  concept %ld  ann_attr_ptrs %ld at %ld",
		i, leaf_node->ctype, leaf_node->concept,
		leaf_node->num_ann_attrs, leaf_node->ann_attr_ptr);
	print_string(linebuf, outbuf);
    }

    /* print tree nodes */
    (void) sprintf(linebuf, "\nTree_nodes (%ld):", adv_vec->num_nodes);
    print_string(linebuf, outbuf);
    for (i = 0; i < adv_vec->num_nodes; i++) {
    	TREE_NODE *tree_node;

	tree_node = &(adv_vec->tree[i]);
	(void) sprintf(linebuf,
		"%d:  info %ld  wt %f  child %ld  sibling %ld",
		i, tree_node->info, tree_node->weight,
		tree_node->child, tree_node->sibling);
	print_string(linebuf, outbuf);
	(void) sprintf(linebuf,
		"\tannotations %ld at %ld",
		tree_node->num_annotations, tree_node->q_annotation);
	print_string(linebuf, outbuf);
    }

    /* print infoneeds */
    (void) sprintf(linebuf, "\nInfo_needs (%ld):", adv_vec->num_infoneeds);
    print_string(linebuf, outbuf);
    for (i = 0; i < adv_vec->num_infoneeds; i++) {
    	INFO_NEED *info_need;

	info_need = &(adv_vec->info_needs[i]);
	(void) sprintf(linebuf,
		"%d:  head_node %ld  wt %f  restrict_set node %ld  docs %ld at %ld",
		i, info_need->head_node, info_need->weight,
		info_need->restrict_term_node,
		info_need->num_docs, info_need->doc_ptr);
	print_string(linebuf, outbuf);
	(void) sprintf(linebuf,
		"\tcollections %ld at %ld  annotations %ld at %ld",
		info_need->num_colls, info_need->collection,
		info_need->num_annotations, info_need->q_annotation);
	print_string(linebuf, outbuf);
    }

    /* print collections */
    (void) sprintf(linebuf, "\nCollections (%ld):", adv_vec->num_collections);
    print_string(linebuf, outbuf);
    for (i = 0; i < adv_vec->num_collections; i++) {
	offset = &(adv_vec->collection_texts[i]);
	(void) sprintf(fmtbuf, "%%d: <%%.%lds> %%d",
				offset->end - offset->begin + 1);
	(void) sprintf(linebuf, fmtbuf, i,
				adv_vec->query_text.buf + offset->begin,
				adv_vec->collection_list[i]);
	print_string(linebuf, outbuf);
    }

    /* print docids */
    (void) sprintf(linebuf, "\nDocids (%ld):", adv_vec->num_docids);
    print_string(linebuf, outbuf);
    for (i = 0; i < adv_vec->num_docids; i++) {
	offset = &(adv_vec->docid_texts[i]);
	eid = &(adv_vec->docid_list[i]);
	(void) sprintf(fmtbuf, "%%d: <%%.%lds> %%ld.%%u.%%u",
				offset->end - offset->begin + 1);
	(void) sprintf(linebuf, fmtbuf, i,
				adv_vec->query_text.buf + offset->begin,
				eid->id, eid->ext.type, eid->ext.num);
	print_string(linebuf, outbuf);
    }

    return (1);
}
