#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libinter/show_doc_parts.c,v 11.2 1993/02/03 16:51:28 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include <ctype.h>
#include <string.h>
#include "common.h"
#include "context.h"
#include "docindex.h"
#include "functions.h"
#include "inter.h"
#include "io.h"
#include "local_eid.h"
#include "local_functions.h"
#include "local_inter.h"
#include "param.h"
#include "proc.h"
#include "smart_error.h"
#include "spec.h"
#include "textloc.h"
#include "tr_vec.h"
#include "retrieve.h"

static long show_high_docs_inst, eid_inst, headings_flag;   
static char *comline[MAX_COMMAND_LINE];

static SPEC_PARAM spec_args[] = {
    {"local.inter.print.headings", getspec_bool, (char *) &headings_flag},
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int num_inst = 0;

extern int init_show_high_doc(), show_high_doc(), close_show_high_doc();


int
init_show_doc_part (spec, unused)
SPEC *spec;
char *unused;
{

    if (num_inst++ > 0) {
        return (num_inst);
    }
    if(UNDEF == lookup_spec(spec, &spec_args[0], num_spec_args))
      return (UNDEF);
    if(UNDEF == (show_high_docs_inst = init_show_high_doc(spec, (char *)NULL)) ||
       UNDEF == (eid_inst = init_eid_util( spec, (char *)NULL )))
      return(UNDEF);
    return (num_inst);
}


int
close_show_doc_part (inst)
int inst;
{
    if (--num_inst > 0)
        return (0);

    if(UNDEF == close_show_high_doc(show_high_docs_inst) ||
       UNDEF == close_eid_util( eid_inst ))
      return(UNDEF);

    return (1);
}


int
show_named_doc_part (is, unused)
INTER_STATE *is;
char *unused;
{
    long i;
    EID_LIST elist;
    STRING_LIST strlist;

    if (is->num_command_line < 2) {
        if (UNDEF == add_buf_string ("No document specified\n", &is->err_buf))
            return (UNDEF);
        return (0);
    }

    strlist.string = comline;
    for (i = 1; i < is->num_command_line; i++)
	strlist.string[i-1] = is->command_line[i];
    strlist.num_strings = is->num_command_line - 1;

    if (UNDEF == stringlist_to_eidlist(&strlist, &elist, eid_inst)) {
	if (UNDEF == add_buf_string ("Bad document\n", &is->err_buf))
	    return (UNDEF);
	return(0);
    }

    for (i=0; i < elist.num_eids; i++)
	if (UNDEF == show_named_eid(is, elist.eid[i])) {
	    if (UNDEF == add_buf_string ("Bad document\n", &is->err_buf))
		return (UNDEF);
	    return(0);
	}

    return(1);
}


int 
show_named_eid(is, eid)
INTER_STATE *is;
EID eid;
{
    char temp_buf[PATH_LEN], *partstr;
    int j;
    SM_BUF sm_buf;
    SM_INDEX_TEXTDOC pp;

    eid_to_string(eid, &partstr);
    if (eid.ext.type == ENTIRE_DOC) {
	sprintf(temp_buf, "\n----- Document %s -----\n", partstr);
	if (headings_flag &&
	    UNDEF == add_buf_string(temp_buf, &is->output_buf))
	    return UNDEF;
	if (UNDEF == show_high_doc(is, eid.id, show_high_docs_inst))
	    return(UNDEF);
    }
    else {
	/* get the text for this part */
	if (UNDEF == eid_textdoc(eid, &pp, eid_inst)) {
	    return (UNDEF);
	}
	for (j = 0; j < pp.mem_doc.num_sections; j++) {
	    if (eid.ext.type == P_SENT)
		sprintf(temp_buf, "\n----- Sentence %s -----\n", partstr);
	    if (eid.ext.type == P_PARA)
		sprintf(temp_buf, "\n----- Paragraph %s -----\n", partstr);
	    if (eid.ext.type == P_SECT)
		sprintf(temp_buf, "\n----- Section %s -----\n", partstr);
	    if (headings_flag &&
		UNDEF == add_buf_string(temp_buf, &is->output_buf))
		return (UNDEF);

	    sm_buf.buf = &pp.doc_text[pp.mem_doc.sections[j].begin_section];
	    sm_buf.end = pp.mem_doc.sections[j].end_section -
		pp.mem_doc.sections[j].begin_section;
	    if (UNDEF == add_buf(&sm_buf, &is->output_buf))
		return (UNDEF);
	}
    }
    return(1);
}
