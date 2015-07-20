#include "common.h"
#include "param.h"
#include "spec.h"
#include "smart_error.h"
#include "functions.h"
#include "trace.h"
#include "inst.h"
#include "proc.h"
#include "docindex.h"
#include "vector.h"
#include "docdesc.h"
#include "infoneed_parse.h"
#include "adv_vector.h"
#include <ctype.h>

static long num_sects;
static SPEC_PARAM spec_args[] = {
    { "index.num_sections",	getspec_long,	(char *) &num_sects },
    TRACE_PARAM ("index.parse.trace")
};
static int num_spec_args = sizeof(spec_args) / sizeof(spec_args[0]);

static char *prefix;
static PROC_TAB *itree_proc;
static SPEC_PARAM_PREFIX prefix_args[] = {
    /* index.section.%ld */
    { &prefix,	"itree_parse",	getspec_func,	(char *) &itree_proc },
};
static int num_prefix_args = sizeof(prefix_args) / sizeof (prefix_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    int num_sections;
    PROC_INST *itree_parser;	/* query->tree */

    int sectid_inst;
    int adv_inst;
    IFN_QUERY query;		/* query being worked on */
    IFN_TREE_NODE ifn_root;	/* root of parse tree */
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int init_ifn_to_advv(), ifn_to_advv(), close_ifn_to_advv();
static void free_ifn_children();


int
init_infoneed_index(spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    STATIC_INFO *ip;
    int new_inst;
    long i;
    char prefix_string[PATH_LEN];

    NEW_INST(new_inst);
    if (UNDEF == new_inst)
	return(UNDEF);
    
    ip = &info[new_inst];

    if (UNDEF == lookup_spec(spec, &spec_args[0], num_spec_args))
	return(UNDEF);
    
    PRINT_TRACE (2, print_string, "Trace: entering init_infoneed_index");

    if (UNDEF == (ip->sectid_inst = init_sectid_num(spec, param_prefix)))
	return(UNDEF);

    ip->num_sections = num_sects;

    if ((PROC_INST *)0 == (ip->itree_parser = (PROC_INST *)malloc(
				(unsigned)num_sects * sizeof(PROC_INST))))
	return(UNDEF);

    for (i = 0; i < num_sects; i++) {
	(void) sprintf(prefix_string, "%sindex.section.%ld.",
					param_prefix ? param_prefix : "", i);
	prefix = prefix_string;
	if (UNDEF == lookup_spec_prefix(spec, &prefix_args[0], num_prefix_args))
	    return(UNDEF);

	ip->itree_parser[i].ptab = itree_proc;
	if (UNDEF == (ip->itree_parser[i].inst =
			    itree_proc->init_proc(spec, prefix_string)))
	    return(UNDEF);
    }

    if (UNDEF == (ip->adv_inst = init_ifn_to_advv(spec, param_prefix)))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_infoneed_index");

    ip->valid_info = 1;

    return(new_inst);
}

int
infoneed_index(pp_vec, adv_vec, inst)
SM_INDEX_TEXTDOC *pp_vec;
ADV_VEC *adv_vec;
int inst;
{
    STATIC_INFO *ip;
    long i;
    long section_num;
    IFN_TREE_NODE *ifn_prev_child, *ifn_this_child;

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "infoneed_index");
	return(UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering infoneed_index");

    /* To combine itrees for various sections, they all have to have
     * the same basis for their offsets, not section-specific ones.
     */
    ip->query.query_text = pp_vec->doc_text;

    init_ifn(&(ip->ifn_root));
    ip->ifn_root.op = IFN_ROOT_OP;

    ifn_prev_child = ifn_this_child = (IFN_TREE_NODE *)0;

    for (i = 0; i < pp_vec->mem_doc.num_sections; i++) {
	/* Get the section number coresponding to this section id */
	if (UNDEF == sectid_num (&pp_vec->mem_doc.sections[i].section_id,
				 &section_num,
				 ip->sectid_inst))
	    return(UNDEF);
	
	/* Construct a query giving the desired section's text, and parse it */
	ip->query.start_pos = pp_vec->mem_doc.sections[i].begin_section;
	ip->query.end_pos = pp_vec->mem_doc.sections[i].end_section - 1;
	if (ip->query.end_pos > ip->ifn_root.text_end)
	    ip->ifn_root.text_end = ip->query.end_pos;
	ip->query.pos = ip->query.start_pos;
	ip->query.ifn_inst = ip->itree_parser[section_num].inst;

	if (UNDEF == infoneed_add_child(&(ip->ifn_root), &ifn_prev_child,
				&ifn_this_child, ip->query.pos))
	    return(UNDEF);

	if (UNDEF == ip->itree_parser[section_num].ptab->proc (&(ip->query),
			ifn_this_child, ip->itree_parser[section_num].inst))
	    return(UNDEF);
    }

    /* convert to adv_vec */
    adv_vec->id_num = pp_vec->id_num;
    adv_vec->query_text.buf = pp_vec->doc_text;
    adv_vec->query_text.end = ip->ifn_root.text_end;	/* size ?? */

    if (UNDEF == ifn_to_advv(&(ip->ifn_root), adv_vec, ip->adv_inst))
	return(UNDEF);
    
    /* free itree */
    free_ifn_children(&(ip->ifn_root));

    PRINT_TRACE (2, print_string, "Trace: leaving infoneed_index");
    return(1);
}

int
close_infoneed_index(inst)
int inst;
{
    STATIC_INFO *ip;
    long i;

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "close_infoneed_index");
	return(UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering close_infoneed_index");

    if (UNDEF == close_sectid_num(ip->sectid_inst))
	return(UNDEF);

    for (i = 0; i < ip->num_sections; i++) {
	if (UNDEF == ip->itree_parser[i].ptab->close_proc(
						ip->itree_parser[i].inst))
	    return(UNDEF);
    }

    (void) free((char *)ip->itree_parser);

    if (UNDEF == close_ifn_to_advv(ip->adv_inst))
	return(UNDEF);

    ip->valid_info = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving close_infoneed_index");

    return(0);
}


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


static void
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
