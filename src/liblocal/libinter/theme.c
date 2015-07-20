#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libinter/theme.c,v 11.2 1993/02/03 16:52:09 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Generate themes for a document.
 *2 theme (is, unused)
 *3   INTER_STATE *is;
 *3   char *unused;
 *4 init_theme (spec, unused)
 *4 close_theme (inst)

 *7 Call theme_core to do the basic work.

***********************************************************************/

#include <ctype.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include "common.h"
#include "compare.h"
#include "context.h"
#include "functions.h"
#include "inst.h"
#include "inter.h"
#include "local_eid.h"
#include "local_functions.h"
#include "param.h"
#include "proc.h"
#include "retrieve.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "vector.h"

static SPEC_PARAM spec_args[] = {
  TRACE_PARAM ("local.inter.theme.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


typedef struct {
    int valid_info;	/* bookkeeping */

    int lcom_parse_inst, theme_inst; 
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static char buf[PATH_LEN];


int
init_theme (spec, unused)
SPEC *spec;
char *unused;
{
    int new_inst;
    STATIC_INFO *ip;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_theme");
    NEW_INST( new_inst );
    if (UNDEF == new_inst)
        return UNDEF;
    ip = &info[new_inst];
    ip->valid_info = 1;

    if (UNDEF==(ip->lcom_parse_inst = init_local_comline_parse(spec, NULL)) ||
	UNDEF==(ip->theme_inst = init_theme_core(spec, NULL)))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_theme");
    return new_inst;
}


int
close_theme (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_theme");
    if (!VALID_INST(inst)) {
        set_error( SM_ILLPA_ERR, "Instantiation", "close_theme");
        return UNDEF;
    }
    ip = &info[inst];

    if (UNDEF == close_local_comline_parse(ip->lcom_parse_inst) ||
	UNDEF == close_theme_core(ip->theme_inst))
	return(UNDEF);
	     
    ip->valid_info = 0;
    PRINT_TRACE (2, print_string, "Trace: leaving close_theme");
    return (0);
}


int
theme (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    char *str;
    int status, i, j;
    COMLINE_PARSE_RES parse_results;
    THEME_RESULTS themes;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering theme");
    if (!VALID_INST(inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "theme");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (1 != (status = local_comline_parse(is, &parse_results, 
					   ip->lcom_parse_inst)))
	return(status);

    if (UNDEF == theme_core(&parse_results, &themes, inst)) {
        if (UNDEF == add_buf_string("Couldn't generate themes\n",
				    &is->err_buf))
            return UNDEF;
        return(0);
    }

    /* Display parameters used. */
    sprintf(buf, "-- Generated %d themes --\n\n", themes.num_themes);
    if (UNDEF == add_buf_string( buf, &is->output_buf ))
	return UNDEF;
    for (i = 0; i < themes.num_themes; i++) { 
	int num_parts = themes.theme_ids[i].num_eids;
	sprintf(buf, "-- Theme %d: %d parts --\n", i+1, num_parts);
	for (j = 0; j < num_parts; j++) {
	    (void) eid_to_string(themes.theme_ids[i].eid[j], &str);
	    strcat(buf, str);
	    ((j + 1) % 5 == 0 && j < num_parts) ? 
		strcat(buf, "\n") : strcat(buf, "\t");
	}
	strcat(buf, "\n\n");
	if (UNDEF == add_buf_string(buf, &is->output_buf))
	    return UNDEF;
    }

    PRINT_TRACE (2, print_string, "Trace: leaving theme"); 
    return(1);
}
