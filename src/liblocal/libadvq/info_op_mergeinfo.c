#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libadvq/info_op_mergeinfo.c,v 11.2 1997/09/17 16:51:18 smart Exp $";
#endif

/* Copyright (c) 1997 - Janet Walz.

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/
/********************   PROCEDURE DESCRIPTION   ************************
 *0 Parse SGML-ish <MERGE_INFO>
 *1 local.infoneed.op.parse.merge_info
 *2 ifn_parse_mergeinfo(ifn_query, ifn_node, inst)
 *3   IFN_QUERY *ifn_query;
 *3   IFN_TREE_NODE *ifn_node;
 *3   int inst;
 *4 init_ifn_parse_mergeinfo(spec, unused)
 *5   "index.parse.trace"
 *4 close_ifn_parse_mergeinfo(inst)

 *7 Parse an SGML-ish MERGE_INFO, as in TIPSTER DN2.  First process a fixed
 *7 set of possible characteristics in the <MERGE_INFO ...> header, then
 *7 call the generic infoneed_parse_data() routine to process child data.
 *7 Allow partial processing of other operators than MERGE_INFO, in case
 *7 they're generic enough to not need routines of their own immediately.
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
#include "vector.h"
#include "infoneed_parse.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("index.parse.trace")
};
static int num_spec_args = sizeof(spec_args) / sizeof(spec_args[0]);

/* characteristics that may be found with this operator */
static char *kaks[] = { "MERGETYPE", (char *)0 };

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_ifn_parse_mergeinfo(spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;

    NEW_INST(new_inst);
    if (UNDEF == new_inst)
	return(UNDEF);
    
    ip = &info[new_inst];

    if (UNDEF == lookup_spec(spec, &spec_args[0], num_spec_args))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_ifn_parse_mergeinfo");

    PRINT_TRACE (2, print_string, "Trace: leaving init_ifn_parse_mergeinfo");

    ip->valid_info = 1;

    return(new_inst);
}

int
ifn_parse_mergeinfo(ifn_query, ifn_node, inst)
IFN_QUERY *ifn_query;
IFN_TREE_NODE *ifn_node;
int inst;
{
    STATIC_INFO *ip;
    int pos, i, retval;
    char c;
    char stop_tag[PATH_LEN];	/* ?? */

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "ifn_parse_mergeinfo");
	return(UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering ifn_parse_mergeinfo");

    retval = 1;

    /* copy starting OP over so we will recognize </OP> */
    pos = ifn_query->pos+1;
    i = 0;
    c = ifn_query->query_text[pos];
    /* this loop is guaranteed safe by previous same_tag() call */
    while (c && !sgml_breakchar(c)) {
	stop_tag[i++] = c;
	pos++;
	c = ifn_query->query_text[pos];
    }
    stop_tag[i] = '\0';

    if (same_tag(stop_tag, "MERGE_INFO")) {
	/* proper op for procedure -- process any characteristics of op */
	if (UNDEF == infoneed_parse_kaks(ifn_query, &pos, kaks, ifn_node))
	    retval = UNDEF;
    } else {
	/* skip any characteristics */
	/* possibly process those with same name?? */
	while (pos < ifn_query->end_pos) {
		c = ifn_query->query_text[pos];
		pos++;
		if (c == '>') break;
	}
    }
    ifn_query->pos = pos;

    if (UNDEF == infoneed_parse_data(ifn_node, stop_tag,
						ifn_query->ifn_inst, FALSE))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving ifn_parse_mergeinfo");
    return(retval);
}

int
close_ifn_parse_mergeinfo(inst)
int inst;
{
    STATIC_INFO *ip;

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "close_ifn_parse_mergeinfo");
	return(UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering close_ifn_parse_mergeinfo");

    ip->valid_info = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving close_ifn_parse_mergeinfo");
    return(0);
}
