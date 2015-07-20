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

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("index.parse.trace")
};
static int num_spec_args = sizeof(spec_args) / sizeof(spec_args[0]);

/* characteristics that may be found with this operator */
static char *kaks[] = { (char *)0 };

/* children that may be found with this operator */
static IFN_CHILDREN generic_kids[] = {
	{ "SE_APP_HELP", IFN_ONE_OPTIONAL, UNDEF },
	{ "SE_APP_EXPL", IFN_ONE_OPTIONAL, UNDEF },
	{ "SE_APP_MATCHES", IFN_ZERO_OR_MORE, UNDEF },
	{ "", IFN_NONE, UNDEF }
};

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    IFN_CHILDREN *kids;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_ifn_parse_seapinfo(spec, char_op_table)
SPEC *spec;
char *char_op_table;
{
    STATIC_INFO *ip;
    int new_inst;

    NEW_INST(new_inst);
    if (UNDEF == new_inst)
	return(UNDEF);
    
    ip = &info[new_inst];

    if (UNDEF == lookup_spec(spec, &spec_args[0], num_spec_args))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_ifn_parse_seapinfo");

    ip->kids = (IFN_CHILDREN *) malloc((unsigned)(sizeof(generic_kids)));
    if ((IFN_CHILDREN *)0 == ip->kids)
	return(UNDEF);
    (void) bcopy((char *)generic_kids, (char *)ip->kids,
				(int)sizeof(generic_kids));
    if (UNDEF == infoneed_specialize_kids(ip->kids, (ADVQ_OP *)char_op_table))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_ifn_parse_seapinfo");

    ip->valid_info = 1;

    return(new_inst);
}

int
ifn_parse_seapinfo(ifn_query, ifn_node, inst)
IFN_QUERY *ifn_query;
IFN_TREE_NODE *ifn_node;
int inst;
{
    STATIC_INFO *ip;
    int pos, i, retval;
    char c;
    char stop_tag[PATH_LEN];	/* ?? */

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "ifn_parse_seapinfo");
	return(UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering ifn_parse_seapinfo");

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

    if (same_tag(stop_tag, "SE_APP_INFO")) {
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

    if (UNDEF == infoneed_parse_args(ifn_node, stop_tag, ifn_query->ifn_inst))
	return(UNDEF);
    
    if (UNDEF == infoneed_check_kids(ifn_node, ip->kids, (IFN_CHILDREN *)0))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving ifn_parse_seapinfo");
    return(retval);
}

int
close_ifn_parse_seapinfo(inst)
int inst;
{
    STATIC_INFO *ip;

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "close_ifn_parse_seapinfo");
	return(UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering close_ifn_parse_seapinfo");

    (void) free((char *)ip->kids);
    ip->valid_info = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving close_ifn_parse_seapinfo");
    return(0);
}
