#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/next_vecid.c,v 11.2 1993/02/03 16:50:55 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/******************** PROCEDURE DESCRIPTION ************************
 *0 Give next valid id for a new document
 *1 index.next_vecid.next_vecid
 *2 next_vecid (unused, id, inst)
 *3   char *unused;
 *3   long *id;
 *3   int inst;
 *4 init_next_vecid (spec, unused)
 *5   "doc.textloc_file"
 *5   "index.next_vecid.trace"
 *4 close_next_vecid (inst)
 *8 Find max docid used in textloc_file, start one higher, and increment by
 *8 1 with each call
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"


/* Docids are assigned sequentially following the last docid previously
   assigned */
static char *textloc_file;

static SPEC_PARAM spec_args[] = {
    {"doc.textloc_file",            getspec_dbfile,   (char *) &textloc_file},
    TRACE_PARAM ("index.next_vecid.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


static long next_id = 0;

int
init_next_vecid (spec, unused)
SPEC *spec;
char *unused;
{
    REL_HEADER *rh;

    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_next_vecid");

    if (next_id > 0) {
        set_error (SM_ILLPA_ERR, "Attempt to re-initialize","init_next_vecid");
        return (UNDEF);
    }

    next_id = 1;
    if (NULL != (rh = get_rel_header (textloc_file)))
        next_id = rh->max_primary_value + 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_next_vecid");

    return (0);
}

int
next_vecid (unused, id, inst)
char *unused;
long *id;
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering next_vecid");

    *id = next_id++;

    PRINT_TRACE (4, print_long, id);
    PRINT_TRACE (2, print_string, "Trace: leaving next_vecid");

    return (0);
}

int
close_next_vecid (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_next_vecid");

    next_id = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving close_next_vecid");

    return (0);
}



