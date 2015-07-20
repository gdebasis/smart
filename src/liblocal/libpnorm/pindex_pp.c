#ifdef RCSID
static char rcsid[] = "$Header: pindex.c,v 8.0 85/06/03 17:38:02 smart Exp $";
#endif

/* Copyright (c) 1984 - Gerard Salton, Chris Buckley, Ellen Voorhees */

#include <stdio.h>
#include "proc.h"
#include "functions.h"
#include "common.h"
#include "param.h"
#include "io.h"
#include "rel_header.h"
#include "dict.h"
#include "vector.h"
#include "pnorm_vector.h"
#include "pnorm_common.h"
#include "pnorm_indexing.h"
#include "spec.h"
#include "trace.h"
#include "local_functions.h"

long no_ctypes;
static PROC_TAB *makevec;
static char *pp_outfile;
static char *pnormvec_file;
static long pnormvec_mode;

static SPEC_PARAM spec_args[] = {
    {"index.num_ctypes",	getspec_long,	 (char *) &no_ctypes},
    {"index.makevec",		getspec_func,	 (char *) &makevec},
    {"index.preparse.pp_outfile",getspec_dbfile,(char *) &pp_outfile},
    {"index.query_file",        getspec_dbfile,  (char *) &pnormvec_file},
    {"index.query_file.rwmode", getspec_filemode,(char *) &pnormvec_mode},
    TRACE_PARAM ("index.index_pp.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static char *prefix;
static SPEC_PARAM_PREFIX p1spec_args[] = {
    {&prefix,	"makevec",	getspec_func,	(char *) &makevec},
};
static int num_p1spec_args = sizeof (p1spec_args) / sizeof (p1spec_args[0]);

static long ctype;
static SPEC_PARAM_PREFIX p2spec_args[] = {
    {&prefix,	"ctype",	getspec_long,	(char *) &ctype},
};
static int num_p2spec_args = sizeof (p2spec_args) / sizeof (p2spec_args[0]);

static char *prefix_ctype;
static char *dict_file;
static long dict_mode;
static SPEC_PARAM_PREFIX p3spec_args[] = {
    {&prefix_ctype, "dict_file",       getspec_dbfile,   (char *) &dict_file},
    {&prefix_ctype, "dict_file.rmode", getspec_filemode, (char *) &dict_mode},
};
static int num_p3spec_args = sizeof (p3spec_args) / sizeof (p3spec_args[0]);

static FILE	*pp_outfile_fp;

PNORM_VEC	qvector;
NODE_INFO	*node_info;
unsigned	node_info_size;
TREE_NODE	*tree;
unsigned	num_tree_nodes;
int		vec_index;
int		dict_index;


int
init_pindex_pp (spec, passed_prefix)
SPEC	*spec;
char	*passed_prefix;
{
    FILE	*fopen();
    char	prefix_buf[48];

    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    /* If a prefix was passed in, use it to look up any prefixed values.
     * Note that these will replace the default values. */
    if (passed_prefix != NULL) {
        (void) sprintf(prefix_buf, "%s.", passed_prefix);
        prefix = prefix_buf;
        if (UNDEF == lookup_spec_prefix (spec, &p1spec_args[0],
                                         num_p1spec_args))
            return (UNDEF);
	if (UNDEF == lookup_spec_prefix (spec, &p2spec_args[0], 
					 num_p2spec_args))
	    return (UNDEF);
    }

    (void) sprintf (prefix_buf, "index.parse.ctype.%ld.", ctype);
    prefix_ctype = prefix_buf;
    if (UNDEF == lookup_spec_prefix (spec, &p3spec_args[0], 
				     num_p3spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_pindex_pp");

    if (NULL == (pp_outfile_fp = fopen(pp_outfile, "r"))) {
        fprintf (stderr, "can't find preparsed output file : %s\n", pp_outfile);
        return (UNDEF);
    }

    if (UNDEF == (dict_index=open_dict(dict_file, dict_mode)))
	return (UNDEF);

    if (UNDEF == create_pnorm(pnormvec_file))
	return (UNDEF);

    if (UNDEF == (vec_index=open_pnorm(pnormvec_file, pnormvec_mode)))
	return (UNDEF);

    node_info_size = MALLOC_SIZE;
    if (NULL == (char *)(node_info = (NODE_INFO *)
        malloc(node_info_size*sizeof(NODE_INFO))))
	return (UNDEF);

    num_tree_nodes = MALLOC_SIZE;
    if (NULL == (char *)(tree = (TREE_NODE *)
        malloc(num_tree_nodes*sizeof(TREE_NODE))))
	return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_pindex_pp");
    return (0);

} /* init_pindex_pp */


int
pindex_pp (unused1, unused2, inst)
char	*unused1;
char	*unused2;
int	inst;
{
    extern int yyparse();
    extern FILE *yyin;

    PRINT_TRACE (2, print_string, "Trace: entering pindex_pp");

    /* create query vectors */
    yyin = pp_outfile_fp;
    (void) yyparse();

    free (node_info);
    free (tree);

/*
    PRINT_TRACE (4, print_vector, vec);
*/
    PRINT_TRACE (2, print_string, "Trace: leaving pindex_pp");
    return (1);

} /* pindex_pp */


int
close_pindex_pp (inst)
int	inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_pindex_pp");

    fclose (pp_outfile_fp);

    if (UNDEF == close_pnorm(vec_index))
	return (UNDEF);

    if (UNDEF == close_dict(dict_index))
	return (UNDEF);

    free (node_info);
    free (tree);

    PRINT_TRACE (2, print_string, "Trace: leaving close_pindex_pp");
    return (0);

} /* close_pindex_pp */

