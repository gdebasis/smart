#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libadvq/infoneed_index.c.c,v 11.2 1997/09/17 16:51:18 smart Exp $";
#endif

/* Copyright (c) 1997 - Janet Walz.

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/
/********************   PROCEDURE DESCRIPTION   ************************
 *0 Parse SGML-ish DN2-like query into an ADV_VEC
 *1 local.infoneed.section.index.standard
 *2 infoneed_index(pp_vec, adv_vec, inst)
 *3   SM_INDEX_TEXTDOC *pp_vec;
 *3   ADV_VEC *adv_vec;
 *3   int inst;
 *4 init_infoneed_index(spec, param_prefix)
 *5   "index.num_sections"
 *5   "index.parse.trace"
 *5   "index.section.*.itree_parse"
 *4 close_infoneed_index(inst)

 *7 Parse an SGML-ish DN2-like query.  First construct an IFN_QUERY from
 *7 the input SM_INDEX_TEXTDOC, then parse each section of the query into
 *7 an IFN_TREE_NODE via the itree_parse procedure, then convert from
 *7 that intermediate form into an ADV_VEC.
 *7 
 *7 1 is returned on normal completion, UNDEF if error.
***********************************************************************/

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
