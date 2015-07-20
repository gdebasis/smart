#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libadvq/infoneed_dummy.c,v 11.2 1997/09/17 16:51:18 smart Exp $";
#endif

/* Copyright (c) 1997 - Janet Walz.

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/
/********************   PROCEDURE DESCRIPTION   ************************
 *0 Parse SGML-ish query section into intermediate IFN_TREE
 *1 local.infoneed.section.ifn_tree.dummy
 *2 infoneed_dummy_parse(query, ifn_root, inst)
 *3   IFN_QUERY *query;
 *3   IFN_TREE_NODE *ifn_root;
 *3   int inst;
 *4 init_infoneed_dummy_parse(spec, unused)
 *5   "index.parse.trace"
 *4 close_infoneed_dummy_parse(inst)

 *7 Parse an SGML-ish query section into an intermediate "tree" consisting
 *7 only of a no-op node.
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
