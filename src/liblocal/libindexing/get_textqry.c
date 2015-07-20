#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/get_textqry.c,v 11.2 1993/02/03 16:49:43 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 
 *1 
 *2 get_textqry (did, textqry, inst)
 *3   long did;
 *3   SM_INDEX_TEXTDOC *textqry;
 *3   int inst;

 *4 init_get_textqry (spec, unused)
 *5   "index.get_textqry.trace"
 *5   "index.get_textqry.preparse"
 *4 close_get_textqry (inst)

 *7 Generates the entire SM_INDEX_TEXTDOC structure for a document
 *7 Return UNDEF if error, else 0;
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "context.h"
#include "proc.h"
#include "sm_display.h"
#include "docindex.h"
#include "inst.h"

static PROC_TAB *preparse;           /* Section preparser */
static PROC_TAB *pp_pp_parts;
static SPEC_PARAM spec_args[] = {
    {"index.get_textqry.named_preparse", getspec_func, (char *) &preparse},
    {"index.get_textqry.pp_pp_parts", getspec_func, (char *) &pp_pp_parts},
    TRACE_PARAM ("convert.get_textqry.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

typedef struct {
    /* bookkeeping */
    int valid_info;

    PROC_TAB *preparse;
    int preparse_inst;
    int pp_pp_para_inst;
    int pp_pp_sent_inst;
    int prev_did;
    SM_INDEX_TEXTDOC pp;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;


int
init_get_textqry (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;
    CONTEXT old_context;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
	return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_get_textqry");

    NEW_INST( new_inst );
    if (UNDEF == new_inst)
        return UNDEF;
    ip = &info[new_inst];

    old_context = get_context();
    set_context (CTXT_QUERY);
    ip->preparse = preparse;
    if (UNDEF == (ip->preparse_inst = ip->preparse->init_proc(spec, "query.")))
        return (UNDEF);
    set_context (CTXT_PARA);
    if (UNDEF == (ip->pp_pp_para_inst = pp_pp_parts->init_proc(spec, "para.")))
        return (UNDEF);
    set_context (CTXT_SENT);
    if (UNDEF == (ip->pp_pp_sent_inst = pp_pp_parts->init_proc(spec, "sent.")))
        return (UNDEF);
    set_context (old_context);

    ip->prev_did = UNDEF;
    ip->valid_info = 1;
    PRINT_TRACE (2, print_string, "Trace: leaving init_get_textqry");

    return new_inst;
}


int
get_textqry (did, textqry, inst)
long did;
SM_INDEX_TEXTDOC *textqry;
int inst;
{
    STATIC_INFO *ip;
    long i, j, k;
    EID doc_eid;

    PRINT_TRACE (2, print_string, "Trace: entering get_textqry");
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "get_textqry");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (did == ip->prev_did) {
        *textqry = ip->pp;
        PRINT_TRACE (2, print_string, "Trace: leaving get_textqry");
        return 0;
    }

    if (ip->prev_did != UNDEF) {
		for (i = 0; i < ip->pp.mem_doc.num_sections; i++) {
	    	for (j = 0; j < ip->pp.mem_doc.sections[i].num_subsects; j++)
				Free(ip->pp.mem_doc.sections[i].subsect[j].subsect);
	    	Free(ip->pp.mem_doc.sections[i].subsect);
		}
    }
    ip->prev_did = did;

    doc_eid.id = did; EXT_NONE(doc_eid.ext);
    if (1 != ip->preparse->proc (&doc_eid, &ip->pp, ip->preparse_inst))
	return UNDEF;

    for (i = 0; i < ip->pp.mem_doc.num_sections; i++) {
	SM_INDEX_TEXTDOC temp_pp = ip->pp, para_pp;

	temp_pp.mem_doc.num_sections = 1;
	temp_pp.mem_doc.sections = &ip->pp.mem_doc.sections[i];
	if (UNDEF == pp_pp_parts->proc (&temp_pp, &para_pp,
					ip->pp_pp_para_inst))
	    return (UNDEF);
	ip->pp.mem_doc.sections[i].num_subsects = para_pp.mem_doc.num_sections;
	if (NULL == (ip->pp.mem_doc.sections[i].subsect = 
		     Malloc(ip->pp.mem_doc.sections[i].num_subsects,
			    SM_DISP_SEC)))
	    return UNDEF;
	Bcopy(para_pp.mem_doc.sections,
	      ip->pp.mem_doc.sections[i].subsect,
	      ip->pp.mem_doc.sections[i].num_subsects*sizeof(SM_DISP_SEC));
	
	for (j = 0; j < ip->pp.mem_doc.sections[i].num_subsects; j++) {
	    SM_INDEX_TEXTDOC sent_pp;

	    temp_pp.mem_doc.num_sections = 1;
	    temp_pp.mem_doc.sections = &ip->pp.mem_doc.sections[i].subsect[j];
	    if (UNDEF == pp_pp_parts->proc (&temp_pp, &sent_pp,
					    ip->pp_pp_sent_inst))
		return (UNDEF);
	    ip->pp.mem_doc.sections[i].subsect[j].num_subsects =
		sent_pp.mem_doc.num_sections;
	    if (NULL == (ip->pp.mem_doc.sections[i].subsect[j].subsect = 
			 Malloc(ip->pp.mem_doc.sections[i].subsect[j].num_subsects,
				SM_DISP_SEC)))
		return UNDEF;
	    Bcopy(sent_pp.mem_doc.sections,
		  ip->pp.mem_doc.sections[i].subsect[j].subsect,
		  ip->pp.mem_doc.sections[i].subsect[j].num_subsects*sizeof(SM_DISP_SEC));
	    for (k = 0;
		 k < ip->pp.mem_doc.sections[i].subsect[j].num_subsects;
		 k++) {
		ip->pp.mem_doc.sections[i].subsect[j].subsect[k].num_subsects = 0;
		ip->pp.mem_doc.sections[i].subsect[j].subsect[k].subsect = NULL;
	    }
	}
    }

    *textqry = ip->pp;
    PRINT_TRACE (2, print_string, "Trace: leaving get_textqry");
    return 0;
}


int
close_get_textqry (inst)
int inst;
{
    int i, j;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_get_textqry");
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_get_textqry");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (ip->prev_did != UNDEF) {
	for (i = 0; i < ip->pp.mem_doc.num_sections; i++) {
	    for (j = 0; j < ip->pp.mem_doc.sections[i].num_subsects; j++)
		Free(ip->pp.mem_doc.sections[i].subsect[j].subsect);
	    Free(ip->pp.mem_doc.sections[i].subsect);
	}
    }

    if (UNDEF == ip->preparse->close_proc (ip->preparse_inst) ||
        UNDEF == pp_pp_parts->close_proc (ip->pp_pp_para_inst) ||
        UNDEF == pp_pp_parts->close_proc (ip->pp_pp_sent_inst))
        return (UNDEF);

    ip->valid_info = 0;
    PRINT_TRACE (2, print_string, "Trace: leaving close_get_textqry");
    return (0);
}
