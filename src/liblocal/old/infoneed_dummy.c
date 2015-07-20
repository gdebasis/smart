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
#include "infoneed_parse.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("index.parse.trace")
};
static int num_spec_args = sizeof(spec_args) / sizeof(spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;


int
init_infoneed_dummy_parse(spec, unused)
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
    
    PRINT_TRACE (2, print_string, "Trace: entering init_infoneed_dummy_parse");

    PRINT_TRACE (2, print_string, "Trace: leaving init_infoneed_dummy_parse");

    ip->valid_info = 1;

    return(new_inst);
}

int
infoneed_dummy_parse(query, ifn_root, inst)
IFN_QUERY *query;
IFN_TREE_NODE *ifn_root;
int inst;
{
    STATIC_INFO *ip;

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "infoneed_dummy_parse");
	return(UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering infoneed_dummy_parse");

    ifn_root->op = IFN_NO_OP;

    PRINT_TRACE (2, print_string, "Trace: leaving infoneed_dummy_parse");
    return(1);
}

int
close_infoneed_dummy_parse(inst)
int inst;
{
    STATIC_INFO *ip;

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "close_infoneed_dummy_parse");
	return(UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering close_infoneed_dummy_parse");

    ip->valid_info = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving close_infoneed_dummy_parse");

    return(0);
}
