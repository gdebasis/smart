#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libadvq/infoneed_parse.c,v 11.2 1997/09/17 16:51:18 smart Exp $";
#endif

/* Copyright (c) 1997 - Janet Walz.

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/
/********************   PROCEDURE DESCRIPTION   ************************
 *0 Parse SGML-ish query section into intermediate IFN_TREE
 *1 local.infoneed.section.ifn_tree.standard
 *2 infoneed_parse(query, ifn_root, inst)
 *3   IFN_QUERY *query;
 *3   IFN_TREE_NODE *ifn_root;
 *3   int inst;
 *4 init_infoneed_parse(spec, param_prefix)
 *5   "index.makevec"
 *5   "index.query.weight"
 *5   "infoneed.num_ops"
 *5   "infoneed.sect_num"
 *5   "index.parse.trace"
 *5   "index.section.*.token_sect"
 *5   "index.section.*.method"
 *5   "infoneed.op.*.name"
 *5   "infoneed.op.*.parse"
 *4 close_infoneed_parse(inst)
 *2 same_tag(s, tag)
 *3   char *s;
 *3   char *tag;
 *2 infoneed_parse_data(ifn_node, stop_tag, inst, astext)
 *3   IFN_TREE_NODE *ifn_node;
 *3   char *stop_tag;
 *3   int inst;
 *3   int astext;
 *2 infoneed_parse_args(ifn_node, stop_tag, inst)
 *3   IFN_TREE_NODE *ifn_node;
 *3   char *stop_tag;
 *3   int inst;

 *7 Parse an SGML-ish query section into an intermediate tree headed by
 *7 ifn_root.  Set up the root node and then start the recursive process
 *7 of calling infoneed_parse_args(), which will identify and call the
 *7 proper operator routine for the current position, which calls back to
 *7 infoneed_parse_args() or infoneed_parse_data() for its arguments.
 *7 1 is returned on normal completion, UNDEF if error.
 *7
 *7 same_tag() does a case-insensitive match of a tag string against the
 *7 beginning of s, returning 1 if it matches and 0 if it doesn't.
 *7
 *7 infoneed_parse_data() uses everything up to stop_tag as direct data/text
 *7 for the SM_BUF associated with ifn_node.  (That is, no further operators
 *7 are looked for.)  If used as text, the text is tokenized and processed
 *7 to find weighted concepts.
 *7 1 is returned on normal completion, UNDEF if error.
 *7
 *7 infoneed_parse_args() is the main workhorse of the recursive decent
 *7 parse, looking for SGML-operator-like things and operator routines that
 *7 process them until it finds stop_tag, which terminates the subtree
 *7 currently being worked on.  Anything that can't be used as the beginning
 *7 ending, or arguments of a proper operator phrase is treated as text.
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
#include "advq_op.h"
#include "infoneed_parse.h"
#include "local_functions.h"
#include <ctype.h>

static PROC_TAB *makevec_proc, *weight_proc;
static long num_ops;
static long sect_num;
static SPEC_PARAM spec_args[] = {
    { "index.makevec",		getspec_func,	(char *) &makevec_proc },
    { "index.query.weight",	getspec_func,	(char *) &weight_proc },
    { "infoneed.num_ops",	getspec_long,	(char *) &num_ops },
    { "infoneed.sect_num",	getspec_long,	(char *) &sect_num },
    TRACE_PARAM ("index.parse.trace")
};
static int num_spec_args = sizeof(spec_args) / sizeof(spec_args[0]);

static char *prefix;
static PROC_TAB *token_proc, *parse_proc;
static SPEC_PARAM_PREFIX prefix_args[] = {
    /* index.section.%ld */
    { &prefix, "token_sect",	getspec_func,	(char *) &token_proc },
    { &prefix, "method",	getspec_func,	(char *) &parse_proc },
};
static int num_prefix_args = sizeof(prefix_args) / sizeof(prefix_args[0]);

static char *op_prefix;
static char *op_name;
static PROC_TAB *op_proc;
static SPEC_PARAM_PREFIX op_spec_args[] = {
    /* infoneed.op.%ld */
    { &op_prefix, "name",     getspec_string, (char *) &op_name },
    { &op_prefix, "parse",    getspec_func,   (char *) &op_proc },
};
static int num_op_spec_args = sizeof(op_spec_args) / sizeof(op_spec_args[0]);


/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    PROC_INST tokenizer;	/* text->token procs */
    PROC_INST parser;		/* token->concepts procs */
    PROC_INST makevec_proc;
    PROC_INST weight_proc;
    int infoneed_sect;		/* fake section to tell tokenizer et al. */

    SPEC *spec;			/* initial specification */
    ADVQ_OP *op_table;		/* table of advq operators */
    int qtext_op_num;		/* position in table of QUOTED_TEXT op */

    IFN_QUERY *query;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static int infoneed_process_text();

int
init_infoneed_parse(spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    STATIC_INFO *ip;
    int new_inst;
    long i;
    char prefix_string[PATH_LEN];
    ADVQ_OP *advq_op;

    NEW_INST(new_inst);
    if (UNDEF == new_inst)
	return(UNDEF);
    
    ip = &info[new_inst];
    ip->spec = spec;

    if (UNDEF == lookup_spec(spec, &spec_args[0], num_spec_args))
	return(UNDEF);
    
    PRINT_TRACE (2, print_string, "Trace: entering init_infoneed_parse");

    ip->makevec_proc.ptab = makevec_proc;
    if (UNDEF == (ip->makevec_proc.inst =
			makevec_proc->init_proc(spec, param_prefix)))
	return(UNDEF);
    ip->weight_proc.ptab = weight_proc;
    if (UNDEF == (ip->weight_proc.inst =
			weight_proc->init_proc(spec, param_prefix)))
	return(UNDEF);


    ip->infoneed_sect = sect_num;
    (void) sprintf(prefix_string, "index.section.%ld.", sect_num);
    prefix = prefix_string;
    /* We don't want to use both param_prefix and prefix_string, or we
     * get index.section.m.index.section.n and won't necessarily get the
     * functions we want.  Maybe we want to use prefix_string above??
     * What about param_prefix for ops?
     */
    if (UNDEF == lookup_spec_prefix(spec, &prefix_args[0], num_prefix_args))
	return(UNDEF);
    
    ip->tokenizer.ptab = token_proc;
    if (UNDEF == (ip->tokenizer.inst =
			token_proc->init_proc (spec, prefix_string)))
	return(UNDEF);
    ip->parser.ptab = parse_proc;
    if (UNDEF == (ip->parser.inst =
			parse_proc->init_proc (spec, prefix_string)))
	return(UNDEF);


    ip->op_table = (ADVQ_OP *)
			malloc ((unsigned) ((num_ops+1) * sizeof(ADVQ_OP)));
    if ((ADVQ_OP *)0 == ip->op_table)
	return(UNDEF);

    op_prefix = prefix_string;
    advq_op = ip->op_table;
    ip->qtext_op_num = UNDEF;
    for (i = 0; i < num_ops; i++) {
	(void) sprintf(prefix_string, "infoneed.op.%ld.", i);
	if (UNDEF == lookup_spec_prefix(spec, &op_spec_args[0],
							num_op_spec_args))
	    return(UNDEF);
	advq_op->op_name = op_name;
	if (!strcmp(op_name, "QUOTED_TEXT"))
	    ip->qtext_op_num = i;
	advq_op->parse_proc.ptab = op_proc;
	advq_op->parse_proc.inst = UNDEF;
	/* do not init procs -- that will be done as needed */
	/* set unused procs to null ?? */
	advq_op++;
    }
    advq_op->op_name = (char *)0;		/* sentinel */
    /*
    if (UNDEF == ip->qtext_op_num) {
	set_error(SM_ILLPA_ERR, "No QUOTED_TEXT operator",
						"init_infoneed_parse");
	return(UNDEF);
    }
    */

    PRINT_TRACE (2, print_string, "Trace: leaving init_infoneed_parse");

    ip->valid_info = 1;

    return(new_inst);
}

int
infoneed_parse(query, ifn_root, inst)
IFN_QUERY *query;
IFN_TREE_NODE *ifn_root;
int inst;
{
    STATIC_INFO *ip;
    IFN_PRINT ifn;

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "infoneed_parse");
	return(UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering infoneed_parse");

    ip->query = query;

    /* Always started with potential implicit SUM, to avoid having to
     * rearrange the tree later if text outside an operator is found.
     * Now, the root is marked as a special non-operator, to allow for e.g.
     * comments alongside the main query tree.
     */
    ifn_root->op = IFN_ROOT_OP;

    if (UNDEF == infoneed_parse_args(ifn_root, "", inst))
	return(UNDEF);

    ifn.ifn_node = ifn_root;
    ifn.query_text = query->query_text;
    PRINT_TRACE (4, print_infoneed, &ifn);

    PRINT_TRACE (2, print_string, "Trace: leaving infoneed_parse");
    return(1);
}

int
close_infoneed_parse(inst)
int inst;
{
    STATIC_INFO *ip;
    ADVQ_OP *advq_op;

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "close_infoneed_parse");
	return(UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering close_infoneed_parse");

    if ((UNDEF == ip->tokenizer.ptab->close_proc(ip->tokenizer.inst)) ||
    	(UNDEF == ip->parser.ptab->close_proc(ip->parser.inst)) ||
	(UNDEF == ip->makevec_proc.ptab->close_proc(ip->makevec_proc.inst)) ||
	(UNDEF == ip->weight_proc.ptab->close_proc(ip->weight_proc.inst)))
	return(UNDEF);

    advq_op = ip->op_table;
    while (advq_op->op_name) {
	if (advq_op->parse_proc.inst != UNDEF)
	    if (UNDEF == advq_op->parse_proc.ptab->close_proc(
						advq_op->parse_proc.inst))
		return(UNDEF);
	advq_op++;
    }
    (void) free((char *)ip->op_table);
    ip->valid_info = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving close_infoneed_parse");

    return(0);
}

/* SGML tags are case-insensitive, so assume that's also the case here */
/* "tag" is an exact tag string; "s" hopefully starts with a tag token */
int
same_tag(s, tag)
char *s, *tag;
{
    char c1, c2;

    while (*tag) {
	if (!*s) return(0);

	/* some old non-ANSI toupper()s don't work on non-islower()s */
	if (islower(*s)) c1 = toupper(*s);
	else c1 = *s;
	if (islower(*tag)) c2 = toupper(*tag);
	else c2 = *tag;

	if (c1 != c2) return(0);

	s++;
	tag++;
    }
    if (*s && !sgml_breakchar(*s))
	return(0);
    return(1);
}

/* tokenize and otherwise process the text buffer associated with ifn_node */
static int
infoneed_process_text(ifn_node, ip)
IFN_TREE_NODE *ifn_node;
STATIC_INFO *ip;
{
    SM_TOKENSECT tokens;
    SM_CONSECT concepts;
    SM_CONDOC condoc;
    SM_VECTOR convec;
    unsigned bytes;

    tokens.section_num = ip->infoneed_sect;
    if (UNDEF == ip->tokenizer.ptab->proc(&(ifn_node->data), &tokens,
							ip->tokenizer.inst))
	return(UNDEF);
    
    /* parse the tokenized text, yielding a list of concept numbers
     * (and locations to be ignored)
     */
    if (UNDEF == ip->parser.ptab->proc(&tokens, &concepts, ip->parser.inst))
	return(UNDEF);
    
    /* make, reweight, and copy over the concept vector */
    condoc.id_num = 42;		/* seems like a fine magic number, and
				 * these itty bitty vectors only live
				 * until the conversion to adv_vec anyway */
    condoc.num_sections = 1;
    condoc.sections = &concepts;
    if (UNDEF == ip->makevec_proc.ptab->proc(&condoc, &convec,
						ip->makevec_proc.inst))
	return(UNDEF);
    if (UNDEF == ip->weight_proc.ptab->proc(&convec, (VEC *)0,
						ip->weight_proc.inst))
	return(UNDEF);

    ifn_node->convec = convec;
    bytes = convec.num_ctype * sizeof(long);
    ifn_node->convec.ctype_len = (long *) malloc(bytes);
    if ((long *)0 == ifn_node->convec.ctype_len)
	return(UNDEF);
    (void) bcopy((char *)convec.ctype_len, (char *)ifn_node->convec.ctype_len,
				(int) bytes);
    bytes = convec.num_conwt * sizeof(CON_WT);
    ifn_node->convec.con_wtp = (CON_WT *) malloc(bytes);
    if ((CON_WT *)0 == ifn_node->convec.con_wtp)
	return(UNDEF);
    (void) bcopy((char *)convec.con_wtp, (char *)ifn_node->convec.con_wtp,
				(int) bytes);

    return(1);
}

/* called with ifn_node->text_start pointing at a non-whitespace; sets text_end
 * after all following non-whitespace
 *
 * probably need to look for op starting within non-whitespace...
 */
static int
infoneed_parse_text(ifn_node, ip)
IFN_TREE_NODE *ifn_node;
STATIC_INFO *ip;
{
    int pos = ifn_node->text_start;
    char c;

    c = ip->query->query_text[pos];
    if ('"' == c) {
	/* it's a shortcut for the QUOTED_TEXT op */
	ifn_node->op = ip->qtext_op_num;
	pos++;
	while (pos < ip->query->end_pos) {
	    c = ip->query->query_text[pos];
	    pos++;
	    if ('"' == c)
		break;
	}
    } else {
	/* just plain text */
	while (pos < ip->query->end_pos) {
	    c = ip->query->query_text[pos];
	    if (whitespace(c))
		break;
	    pos++;
	}
    }
    ifn_node->text_end = pos - 1;

    /* construct a SM_BUF giving the text */
    ifn_node->data.buf = ip->query->query_text + ifn_node->text_start;
    ifn_node->data.end = ifn_node->text_end - ifn_node->text_start + 1;
    if (ifn_node->op == ip->qtext_op_num) {
	/* don't tokenize quotes */
	ifn_node->data.buf++;
	ifn_node->data.end -= 2;
    }

    if (UNDEF == infoneed_process_text(ifn_node, ip))
	return(UNDEF);

    return(1);
}


/* specialized version of infoneed_parse_args() that uses everything up
 * to stop_tag as data/text for ifn_node's SM_BUF
 */
int
infoneed_parse_data(ifn_node, stop_tag, inst, astext)
IFN_TREE_NODE *ifn_node;	/* childless node ready for data/text */
char *stop_tag;
int inst, astext;
{
    STATIC_INFO *ip;
    int pos;

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "infoneed_parse_data");
	return(UNDEF);
    }
    ip = &info[inst];

    ifn_node->data.buf = ip->query->query_text + ip->query->pos;

    while (ip->query->pos < ip->query->end_pos) {

	if ('<' == ip->query->query_text[ip->query->pos]) {
	    /* look for </stop_tag[/b/t/n]*> */
	    pos = ip->query->pos;
	    if (pos + strlen(stop_tag) + 2 < ip->query->end_pos) {
		if ('/' == ip->query->query_text[pos+1]) {
		    if (same_tag(&(ip->query->query_text[pos+2]), stop_tag)) {

			pos += strlen(stop_tag) + 2;
			while (pos < ip->query->end_pos) {
			    char c = ip->query->query_text[pos];

			    if ('>' == c) {
				/* have stop_tag -- process preceding text */
				ifn_node->text_end = pos;
				ifn_node->data.end = 1 + (ip->query->pos - 1) -
				   (ifn_node->data.buf - ip->query->query_text);
				if (astext) {
				    if (UNDEF == infoneed_process_text(ifn_node,
								    ip))
					return(UNDEF);
				}
				ip->query->pos = pos + 1;
				return(1);
			    } else if (whitespace(c)) {
				/* still could be stop_tag */
				pos++;
			    } else
				/* not stop_tag after all */
				break;	/* inner while */
			}
		    }
		}
	    }
	}

	ip->query->pos++;
    }

    return(UNDEF);
}


/* A op parsing procedure is called with the current position on the '<' of
 * "<OP", and a fresh tree node of OP's type.  It will note the "OP",
 * process characteristics if it thinks the OP is its own, and add subnodes
 * of other recognized <OP2 operators and of text/unrecognized-operator-like-
 * things to its node, returning when it finds a matching "</OP[\b\t\n]*>"
 * with the current position set just past the '>'.  Noting the actual OP
 * allows a procedure to have other operators pointed at it for whatever
 * reason (such as their procedures not being written yet).
 *
 * The tree node's text_start is set by the calling procedure; the parsing
 * procedure sets text_end to its ending position.
 */

/* intermediary routine for recursive descent parse, taking care of basic
 * housekeeping so each individual routine can worry only about recognizing
 * its particular operator.
 */
int
infoneed_parse_args(ifn_node, stop_tag, inst)
IFN_TREE_NODE *ifn_node;	/* childless node ready for offspring */
char *stop_tag;		/* add children to ifn_node until </stop_tag > */
int inst;
{
    STATIC_INFO *ip;
    ADVQ_OP *advq_op;
    IFN_TREE_NODE *ifn_prev_child, *ifn_this_child;
    int pos, advq_op_num, ret;
    char *cp;

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "infoneed_parse_args");
	return(UNDEF);
    }
    ip = &info[inst];

    ifn_prev_child = ifn_this_child = (IFN_TREE_NODE *)0;

    /* ifn_node->text_start is beginning of text for node;
     * ip->query->pos is beginning of text past leading <OP...> (if any)
     */
    while (ip->query->pos < ip->query->end_pos) {

	switch (ip->query->query_text[ip->query->pos]) {
	case ' ':
	case '\t':
	case '\n':
	    ip->query->pos++;
	    break;

	case '<':	/* recognize as stop_tag, op, or text */

	    /* look for </stop_tag[/b/t/n]*> */
	    pos = ip->query->pos;
	    if (pos + strlen(stop_tag) + 2 < ip->query->end_pos) {
		if ('/' == ip->query->query_text[pos+1]) {
		    if (same_tag(&(ip->query->query_text[pos+2]), stop_tag)) {

			pos += strlen(stop_tag) + 2;
			while (pos < ip->query->end_pos) {
			    char c = ip->query->query_text[pos];

			    if ('>' == c) {
				/* have stop_tag */
				ifn_node->text_end = pos;
				ip->query->pos = pos + 1;
				return(1);
			    } else if (whitespace(c)) {
				/* still could be stop_tag */
				pos++;
			    } else
				/* not stop_tag after all */
				break;	/* inner while */
			}
		    }
		    if (UNDEF == infoneed_add_child(ifn_node, &ifn_prev_child,
					&ifn_this_child, ip->query->pos))
			return(UNDEF);

		    /* anything else starting with </is text, if not an
		     * outright error
		     */
		    ifn_this_child->op = IFN_TEXT_OP;
		    if (UNDEF == infoneed_parse_text(ifn_this_child, ip))
			return(UNDEF);
		    ip->query->pos = ifn_this_child->text_end + 1;
		    break;	/* switch */
		}
	    }

	    if (UNDEF == infoneed_add_child(ifn_node, &ifn_prev_child,
					&ifn_this_child, ip->query->pos))
		return(UNDEF);

	    /* look for op */
	    pos = ip->query->pos;
	    advq_op = ip->op_table;
	    cp = &(ip->query->query_text[pos+1]);
	    advq_op_num = 0;
	    while (advq_op->op_name) {
		if (pos + strlen(advq_op->op_name) + 1 < ip->query->end_pos
					&& same_tag(cp, advq_op->op_name))
		    break;	/* inner while */
		advq_op++;
		advq_op_num++;
	    }

	    if (!advq_op->op_name) {
		/* anything else starting with < is text, if not an
		 * outright error
		 */
		ifn_this_child->op = IFN_TEXT_OP;
		if (UNDEF == infoneed_parse_text(ifn_this_child, ip))
		    return(UNDEF);
		ip->query->pos = ifn_this_child->text_end + 1;
		break;	/* switch */
	    }

	    /* found op advq_op_num */
	    if (UNDEF == advq_op->parse_proc.inst) {
		advq_op->parse_proc.inst =
		    advq_op->parse_proc.ptab->init_proc(ip->spec,
							(char *)ip->op_table);
		if (UNDEF == advq_op->parse_proc.inst)
		    return(UNDEF);
	    }

	    ifn_this_child->op = advq_op_num;
	    ret = advq_op->parse_proc.ptab->proc(ip->query, ifn_this_child,
						advq_op->parse_proc.inst);
	    if (UNDEF == ret)
		return(UNDEF);
	    else if (0 == ret) {
		/* didn't parse properly as op, treat as text, reusing child */
		ifn_this_child->op = IFN_TEXT_OP;
		if (UNDEF == infoneed_parse_text(ifn_this_child, ip))
		    return(UNDEF);
	    }
	    
	    ip->query->pos = ifn_this_child->text_end + 1;
	    break;	/* switch */

	default:	/* recognize as text */
	    if (UNDEF == infoneed_add_child(ifn_node, &ifn_prev_child,
					&ifn_this_child, ip->query->pos))
		return(UNDEF);
	    ifn_this_child->op = IFN_TEXT_OP;
	    if (UNDEF == infoneed_parse_text(ifn_this_child, ip))
		return(UNDEF);
	    ip->query->pos = ifn_this_child->text_end + 1;
	    break;	/* switch */
	}
    }

    ifn_node->text_end = ip->query->pos - 1;
    if (!*stop_tag)	/* weren't supposed to find an ending tag */
	return(1);
    else
	return(UNDEF);
}
