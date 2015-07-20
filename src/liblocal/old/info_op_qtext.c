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
static char *kaks[] = { (char *)0 };

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_ifn_parse_qtext(spec, unused)
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

    PRINT_TRACE (2, print_string, "Trace: entering init_ifn_parse_qtext");

    PRINT_TRACE (2, print_string, "Trace: leaving init_ifn_parse_qtext");

    ip->valid_info = 1;

    return(new_inst);
}

int
ifn_parse_qtext(ifn_query, ifn_node, inst)
IFN_QUERY *ifn_query;
IFN_TREE_NODE *ifn_node;
int inst;
{
    STATIC_INFO *ip;
    int pos, i, retval;
    char c;
    char stop_tag[PATH_LEN];	/* ?? */

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "ifn_parse_qtext");
	return(UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering ifn_parse_qtext");

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

    if (same_tag(stop_tag, "QUOTED_TEXT")) {
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
						ifn_query->ifn_inst, TRUE))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving ifn_parse_qtext");
    return(retval);
}

int
close_ifn_parse_qtext(inst)
int inst;
{
    STATIC_INFO *ip;

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "close_ifn_parse_qtext");
	return(UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering close_ifn_parse_qtext");

    ip->valid_info = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving close_ifn_parse_qtext");
    return(0);
}
