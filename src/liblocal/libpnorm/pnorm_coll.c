#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/pnorm_coll.c,v 11.2 1993/02/03 16:50:51 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "docindex.h"
#include "context.h"
#include "spec.h"
#include "trace.h"

static char *pp_infile;		/* Location of files to index */
static PROC_INST stemmer,	/* stemming		*/
    pp,				/* Preparse procedures	*/
    index_pp;			/* Convert preparsed text to vector */

static SPEC_PARAM spec_args[] = {
    {"index.preparse.query_loc",getspec_string,(char *) &pp_infile},
    {"index.query.stemmer",	getspec_func, (char *) &stemmer.ptab},
    {"index.query.preparse",    getspec_func, (char *) &pp.ptab},
    {"index.query.index_pp",    getspec_func, (char *) &index_pp.ptab},
    TRACE_PARAM ("index.trace")
    };
static int num_spec_args = sizeof(spec_args) / sizeof(spec_args[0]);

/* Used externally */
SPEC	*pnorm_coll_spec;

int
init_index_pnorm_coll (spec, unused)
SPEC *spec;
char *unused;
{
    CONTEXT old_context;

    pnorm_coll_spec = spec;

    /* Set context indicating that we are indexing a query.  Tells
       inferior procedures to use params appropriate for query instead of
       doc */
    old_context = get_context();
    set_context (CTXT_INDEXING_QUERY);
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_index_pnorm_coll");

    /* Call initialization procedures for preparse */
    if (UNDEF == (stemmer.inst = 
		stemmer.ptab->init_proc (spec, (char *)NULL)) ||
	UNDEF == (pp.inst = pp.ptab->init_proc (spec, pp_infile)))
	return (UNDEF);

    /* Preparse pnorm queries */
    if (UNDEF == pp.ptab->proc((char *)NULL, (char *)NULL, pp.inst))
        return (UNDEF);

    /* Call closing procedures for preparse */
    if (UNDEF == stemmer.ptab->close_proc (stemmer.inst) ||
	UNDEF == pp.ptab->close_proc (pp.inst))
        return (UNDEF);

    /* Call initialization procedure for indexer */
    if (UNDEF == (index_pp.inst =
		index_pp.ptab->init_proc (spec, (char *)NULL)))
        return (UNDEF);

    set_context (old_context);

    PRINT_TRACE (2, print_string, "Trace: leaving init_index_pnorm_coll");
    return (0);
} /* init_index_pnorm_coll */


int
index_pnorm_coll (unused1, unused2, inst)
char *unused1, *unused2;
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering index_pnorm_coll");

    /* Index preparsed pnorm queries */
    if (UNDEF == index_pp.ptab->proc 
		((char *)NULL, (char *)NULL, index_pp.inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving index_pnorm_coll");
    return (1);
} /* index_pnorm_coll */


int
close_index_pnorm_coll (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_index_pnorm_coll");

    if (UNDEF == index_pp.ptab->close_proc(index_pp.inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_index_pnorm_coll");
    return (0);
} /* close_index_pnorm_coll */


#define	MAXLINE		81
static char    termval[MAXLINE];        /* term after stemming */

char *
lookup(istring,ct)      /* stem istring using 'stem_type' method, return stem */
char    istring[];
int     ct;             /* concept type */
{
    char    *stemmed_word;

    /* perform stemming or plural removal as requested */
    if (UNDEF == stemmer.ptab->proc(istring, &stemmed_word, stemmer.inst)) {
          fprintf(stderr,"LOOKUP: illegal stem option setting - quit\n");
          exit(1);
    }

    strcpy (termval,stemmed_word);
    return (termval);
} /* lookup */

