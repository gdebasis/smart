#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/post_pp.c,v 11.0 1992/07/21 18:21:42 chrisb Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Post pre-parser for documents with sections.
 *1 local.index.post_pp.initial
 *2 post_pp_initial(input_doc, output_doc, inst)
 *3   SM_INDEX_TEXTDOC *input_doc;
 *3   SM_INDEX_TEXTDOC *output_doc;
 *3   int inst;
 *4 init_post_pp_initial(spec_ptr, unused)
 *4 close_post_pp_initial(inst)

 *7 Repeat initial chunk of document at end of preparser output. This
 *7 initial chunk will later be handled specially.
 *9 Repeated chunks do not keep subsection information.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "spec.h"
#include "preparser.h"
#include "smart_error.h"
#include "functions.h"
#include "docindex.h"
#include "trace.h"
#include "context.h"
#include "inst.h"
#include "proc.h"

static float init_percent;
static char *section_name;
static SPEC_PARAM pp[] = {
    {"post_pp.init_percent", getspec_float, (char *)&init_percent},
    {"post_pp.section_name", getspec_string, (char *)&section_name},
    TRACE_PARAM ("index.post_pp.trace")
};
static int num_pp = sizeof(pp) / sizeof(pp[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;
    SM_DISP_SEC *mem_disp;        /* Section occurrence info for doc giving
                                     byte offsets within buf */
    unsigned max_num_sections;    /* max current storage reserved for
                                     mem_disp */
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;


int
init_post_pp_initial(spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];

    if (UNDEF == lookup_spec (spec, &pp[0], num_pp))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_post_pp_initial");

    ip->max_num_sections = 10;
    if (NULL == (ip->mem_disp = Malloc(ip->max_num_sections, SM_DISP_SEC)))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_post_pp_initial");

    ip->valid_info = 1;

    return (new_inst);
}

int
post_pp_initial(input_doc, output_doc, inst)
SM_INDEX_TEXTDOC *input_doc;
SM_INDEX_TEXTDOC *output_doc;
int inst;
{
    long num_sec, curr_size, sec_size, target_size, i;
    STATIC_INFO *ip;

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "post_pp_initial");
        return (UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering post_pp_initial");

    /* Copy information from input_doc to output_doc */
    if (NULL == output_doc)
	output_doc = input_doc;
    output_doc->id_num = input_doc->id_num;
    output_doc->doc_text = input_doc->doc_text;
    output_doc->textloc_doc = input_doc->textloc_doc;
    output_doc->mem_doc = input_doc->mem_doc;

    /* Allocate enough space to hold duplicated initial sections */
    num_sec = input_doc->mem_doc.num_sections;
    if (2 * num_sec > ip->max_num_sections) {
	Free(ip->mem_disp);
	ip->max_num_sections = 2 * input_doc->mem_doc.num_sections;
	if (NULL == (ip->mem_disp = Malloc(ip->max_num_sections, SM_DISP_SEC)))
	    return(UNDEF);
    }

    /* Copy over sections */
    for (i = 0; i < num_sec; i++)
	ip->mem_disp[i] = input_doc->mem_doc.sections[i];

    /* Duplicate initial init_percent bytes */
    target_size = init_percent * (output_doc->textloc_doc.end_text - output_doc->textloc_doc.begin_text) / 100.0;
    for (curr_size = 0, i = 0; curr_size < target_size && i < num_sec; i++) {
	ip->mem_disp[num_sec + i] = input_doc->mem_doc.sections[i];
	ip->mem_disp[num_sec + i].section_id = section_name[0];
	sec_size = input_doc->mem_doc.sections[i].end_section -
	    input_doc->mem_doc.sections[i].begin_section;
	ip->mem_disp[num_sec + i].end_section =
	    input_doc->mem_doc.sections[i].begin_section + 
	    MIN(sec_size, target_size - curr_size);
	curr_size += MIN(sec_size, target_size - curr_size);
	ip->mem_disp[num_sec + i].num_subsects = 0;
    }

    output_doc->mem_doc.num_sections = num_sec + i;
    output_doc->mem_doc.sections = ip->mem_disp;

    PRINT_TRACE (4, print_int_textdoc, output_doc);
    PRINT_TRACE (2, print_string, "Trace: leaving post_pp_initial");

    return (1);
}

int
close_post_pp_initial(inst)
int inst;
{
    STATIC_INFO *ip;

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_post_pp_initial");
        return (UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering close_post_pp_initial");
    ip->valid_info--;
    Free(ip->mem_disp);
    PRINT_TRACE (2, print_string, "Trace: leaving close_post_pp_initial");

    return (0);
}
