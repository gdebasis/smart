#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libinter/local_comline_parse.c,v 11.2 1993/02/03 16:52:09 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Parse the command line for a number of interactive commands.
 *2 local_comline_parse (is, parse_results, inst)
 *3   INTER_STATE *is;
 *3   COMLINE_PARSE_RES *parse_results;
 *3   int inst;
 *4 init_local_comline_parse (spec, unused)
 *4 close_local_comline_parse (inst)

 *7 The command line options given to a Compare/Segment/Theme/Path command 
 *7 are parsed. Parameter settings are passed to the calling procedure which
 *7 updates them. Documents specified are set up for pairwise comparison.

 *7 As a special feature, if parentheses are used to group a set of documents,
 *7 the docs in the parens will not be compared to each other. So '1 2 3 4' 
 *7 will cause 6 comparison pairs, but the string '(1 2) (3 4)' will only 
 *7 cause 2.  

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
#include "local_inter.h"
#include "param.h"
#include "proc.h"
#include "retrieve.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "vector.h"

static SPEC_PARAM spec_args[] = {
  TRACE_PARAM ("local.inter.comline_parse.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

typedef struct {
  int valid_info;	/* bookkeeping */

  int eid_inst;

  int max_settings;
  PARAM_SETTINGS settings;
  int max_eids;
  EID_LIST eidlist;
  int max_results;
  ALT_RESULTS altres;

} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;


int
init_local_comline_parse (spec, unused)
SPEC *spec;
char *unused;
{
    int new_inst;
    STATIC_INFO *ip;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_local_comline_parse");

    NEW_INST( new_inst );
    if (UNDEF == new_inst)
        return UNDEF;
    ip = &info[new_inst];
    ip->valid_info = 1;

    if (UNDEF == (ip->eid_inst = init_eid_util(spec, (char *) NULL)))
	return(UNDEF);

    ip->max_settings = 0;
    ip->max_eids = 0; 
    ip->max_results = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving init_local_comline_parse");
    return new_inst;
}


int
close_local_comline_parse (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_local_comline_parse");
    if (!VALID_INST(inst)) {
        set_error( SM_ILLPA_ERR, "Instantiation", "close_local_comline_parse");
        return UNDEF;
    }
    ip = &info[inst];

    if (UNDEF == close_eid_util(ip->eid_inst))
	return(UNDEF);

    if(ip->max_settings) Free(ip->settings.modifiers);
    if(ip->max_eids) Free(ip->eidlist.eid);
    if(ip->max_results) Free(ip->altres.results); 

    ip->valid_info = 0;
    PRINT_TRACE (2, print_string, "Trace: leaving close_local_comline_parse");
    return (0);
}


int
local_comline_parse (is, parse_results, inst)
INTER_STATE *is;
COMLINE_PARSE_RES *parse_results;
int inst;
{
    STATIC_INFO *ip;
    char is_query, reset_frozen_point, *string_id;
    int frozen_point, num_eids, i, j, k;
    EID *eptr;
    EID_LIST doclist;
    STRING_LIST strlist;
    RESULT_TUPLE *rptr;

    PRINT_TRACE (2, print_string, "Trace: entering local_comline_parse");
    if (!VALID_INST(inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "local_comline_parse");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Reserve space for the worst case. */
    if (ip->max_eids < is->num_command_line - 1) {
	if (ip->max_eids) Free(ip->eidlist.eid);
	ip->max_eids += is->num_command_line - 1;
	if (NULL == (ip->eidlist.eid = Malloc(ip->max_eids, EID)))
	    return UNDEF;
    }
    if (ip->max_settings < is->num_command_line) {
	if (ip->max_settings) Free(ip->settings.modifiers);
	ip->max_settings = is->num_command_line;
	if (NULL == 
	    (ip->settings.modifiers = Malloc(ip->max_settings, char *)))
	    return(UNDEF);
    }
    if (ip->max_results < ip->max_eids * ip->max_eids) {
	if (ip->max_results) Free(ip->altres.results);
	ip->max_results = ip->max_eids * ip->max_eids;
	if (NULL == 
	    (ip->altres.results = Malloc(ip->max_results, RESULT_TUPLE)))
	    return UNDEF;
    }

    ip->settings.num_mods = 0; ip->eidlist.num_eids = 0; 
    ip->altres.num_results = 0;
    frozen_point = UNDEF;

    /* Parse command line to generate parameter settings and pairs of 
     * documents that have to be compared. 
     */
    for (i=1; i < is->num_command_line; i++) {
	string_id = is->command_line[i]; 
	is_query = FALSE; reset_frozen_point = FALSE;

	if (0 == strcasecmp(string_id, "same") || 
	    0 == strcasecmp(string_id, "fig") || 
	    string_id[strlen(string_id)-1] == '%') {
	    ip->settings.modifiers[ip->settings.num_mods++] = 
		is->command_line[i];
	    continue;
	}

	if (isalpha(*string_id) && *string_id != 'Q' && i < is->num_command_line - 1) {
	    /* parameter setting */
	    ip->settings.modifiers[ip->settings.num_mods++] = 
		is->command_line[i++];
	    ip->settings.modifiers[ip->settings.num_mods++] = 
		is->command_line[i];
	    continue;
	}

	if (*string_id == '(') {
	    if (frozen_point != UNDEF) {
		if (UNDEF == add_buf_string("Parentheses may not be nested\n",
					    &is->err_buf))
		    return UNDEF;
		return 0;
	    }
	    frozen_point = ip->eidlist.num_eids;
	    string_id++;		/* omit '(' */
	}
	if (*string_id == ')') {
	    if (frozen_point == UNDEF) {
		if (UNDEF == add_buf_string("Unbalanced parantheses\n",
					    &is->err_buf))
		    return UNDEF;
		return 0;
	    }
	    frozen_point = UNDEF;
	    string_id++;		/* omit ')' */
	}
	if (!*string_id) continue;	/* paren was all alone */
	if (string_id[strlen(string_id) - 1] == ')') {
	    reset_frozen_point = TRUE;
	    string_id[strlen(string_id) - 1] = '\0';	/* strip ')' */
	}
	  
	if (*string_id == 'Q') {
	    is_query = TRUE;
	    string_id++;
	}

	/* Get the eid(s) corresponding to this string. */
	strlist.num_strings = 1;
	if (NULL == (strlist.string = Malloc(1, char *))) 
	    return(UNDEF);
	strlist.string[0] = string_id;
	if (UNDEF==stringlist_to_eidlist(&strlist, &doclist, ip->eid_inst)) {
	    if (UNDEF == add_buf_string ("Bad document\n", &is->err_buf))
		return (UNDEF);
	    return(0);
	}
	Free(strlist.string);

	/* Put this (these) eid(s) in the eid list and set up comparisons
	 * for it (them) with other eids. */
	if (ip->eidlist.num_eids + doclist.num_eids > ip->max_eids) {
	    ip->max_eids += doclist.num_eids;
	    if (NULL == (ip->eidlist.eid = Realloc(ip->eidlist.eid, 
						   ip->max_eids, EID)))
		return UNDEF;
            ip->max_results = ip->max_eids * ip->max_eids;
            if (NULL == (ip->altres.results = Realloc(ip->altres.results, 
						      ip->max_results, 
						      RESULT_TUPLE)))
		return UNDEF;
	}
	
	num_eids = ip->eidlist.num_eids;
	rptr = ip->altres.results + ip->altres.num_results;
	for (k = 0; k < doclist.num_eids; k++) {
	    ip->eidlist.eid[num_eids] = doclist.eid[k];
	    if (is_query)
		if (ip->eidlist.eid[num_eids].ext.type != ENTIRE_DOC) {
		    if (UNDEF == add_buf_string("Cannot do query parts",
						&is->err_buf))
			return UNDEF;
		    return 0;
		}
		else ip->eidlist.eid[num_eids].ext.type = QUERY;

	    for (j = 0; 
		 j < (frozen_point==UNDEF ? num_eids : frozen_point); j++) {
		if (is_query && ip->eidlist.eid[j].ext.type == QUERY) {
		    if (UNDEF == add_buf_string("Cannot compare two queries",
						&is->err_buf))
			return UNDEF;
		    return 0;
		}
		rptr->qid = ip->eidlist.eid[j];
		rptr->did = ip->eidlist.eid[num_eids];
		rptr->sim = 0.01;
		rptr++;
	    } /* for (j = 0; ... */
	    num_eids++;	   
	} /* for (k = 0; ... */

	ip->eidlist.num_eids = num_eids;
	ip->altres.num_results = rptr - ip->altres.results;

	if (reset_frozen_point) frozen_point = UNDEF;
    }

    /* Some elementary cleanup: remove duplicate eids and pairs. */
    qsort(ip->eidlist.eid, ip->eidlist.num_eids, sizeof(EID), compare_eids);
    for (i = 0, eptr = ip->eidlist.eid + 1; 
	 eptr < ip->eidlist.eid + ip->eidlist.num_eids; eptr++)
	if (!EID_EQUAL((*eptr), ip->eidlist.eid[i]))
	    ip->eidlist.eid[++i] = *eptr;
    ip->eidlist.num_eids = i+1;

    for (rptr = ip->altres.results; 
	 rptr < ip->altres.results + ip->altres.num_results; rptr++)
	if (eid_greater(&rptr->qid, &rptr->did)) {
	    EID tmp_eid;
	    tmp_eid = rptr->qid;
	    rptr->qid = rptr->did;
	    rptr->did = tmp_eid;
	}
    qsort((char *)ip->altres.results, ip->altres.num_results, 
	  sizeof(RESULT_TUPLE), compare_rtups);
    for (i = 0, rptr = ip->altres.results + 1; 
	 rptr < ip->altres.results + ip->altres.num_results; rptr++)
	if (!EID_EQUAL(rptr->qid, ip->altres.results[i].qid) ||
	    !EID_EQUAL(rptr->did, ip->altres.results[i].did)) {
	    ip->altres.results[++i].qid = rptr->qid; 
	    ip->altres.results[i].did = rptr->did;
	}
    ip->altres.num_results = i+1;

    parse_results->params = &ip->settings;
    parse_results->eids = &ip->eidlist;
    parse_results->res = &ip->altres;

    PRINT_TRACE (2, print_string, "Trace: leaving local_comline_parse");
    return(1);
}
