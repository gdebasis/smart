#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/pp_parts.c,v 11.2 1993/02/03 16:51:02 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/
/********************   PROCEDURE DESCRIPTION   ************************
 *0 pre-parser front-end that constructs overlapping text windows
 *1 local.index.parts_preparse.window
 *2 pp_window (input_doc, out_pp, inst)
 *3   TEXTLOC *input_doc;
 *3   SM_INDEX_TEXTDOC *output_doc;
 *3   int inst;
 *4 init_pp_window (spec_ptr, pp_infile)
 *5   "index.parts_preparse.trace"
 *5   "parts_preparse.preparse_window_size"
 *5   "pp_parts.preparse"
 *4 close_pp_window (inst)

 *7 Puts a preparsed document in output_doc which corresponds to either
 *7 the input_doc (if non-NULL), or the next document found from the list of
 *7 documents in doc_loc (or query_loc if indexing query)
 *7 Returns 1 if found doc to preparse, 0 if no more docs, UNDEF if error

 *8 Calls the pp_parts.preparse preparser to actually preparse input_doc,
 *8 and then constructs overlapping text windows, ignoring the
 *8 original sections. SEE BUG

 *9  BUG: TREC specific hack: join adjacent sections of same section_name
 *9 into one section beofre breaking into windows.
 *  Debasis: Introduce a configuration parameter to control overlapping
 *  of windows. In SBQE, we don't require overlapping windows.
***********************************************************************/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "io.h"
#include "inst.h"
#include "proc.h"
#include "sm_display.h"
#include "docindex.h"
#include "context.h"

static PROC_TAB *pp;        /* Preparsing procedure */
static long window_size;
static char overlap;

static SPEC_PARAM spec_args[] = {
    {"pp_parts.preparse",         getspec_func,    (char *) &pp},
    {"parts_preparse.preparse_window_size", getspec_long,(char *)&window_size},
    {"parts_preparse.preparse_window_overlap", getspec_bool,(char *)&overlap},
    TRACE_PARAM ("index.parts_preparse.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    PROC_INST pp;

    SM_DISP_SEC *sections;
    int num_sections;
    int max_sections;

    char *new_doc_text;
    int max_new_doc_text;
    SM_DISP_SEC *new_doc_sections;
    int max_new_doc_sections;

} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static int new_section(), parse_section(), pp_pp_window();

int
init_pp_window (spec, pp_infile)
SPEC *spec;
char *pp_infile;
{
    STATIC_INFO *ip;
    int new_inst;

    PRINT_TRACE (2, print_string, "Trace: entering init_pp_window");
    
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];

    ip->max_sections = 100;
    if (NULL == (ip->sections = (SM_DISP_SEC *)
                 malloc ((unsigned) ip->max_sections * sizeof (SM_DISP_SEC))))
        return (UNDEF);
    ip->max_new_doc_text = 100000;
    if (NULL == (ip->new_doc_text = (char *)
                 malloc ((unsigned) ip->max_new_doc_text)))
        return (UNDEF);
    ip->max_new_doc_sections = 100;
    if (NULL == (ip->new_doc_sections = (SM_DISP_SEC *)
          malloc ((unsigned) ip->max_new_doc_sections * sizeof (SM_DISP_SEC))))
        return (UNDEF);


    /* Call all initialization procedures for indexing */
    ip->pp.ptab = pp;
    if (UNDEF == (ip->pp.inst = pp->init_proc (spec, pp_infile)))
        return (UNDEF);

    ip->valid_info = 1;
             
    PRINT_TRACE (2, print_string, "Trace: leaving init_pp_window");

    return (new_inst);
}

int
pp_window (input_doc, out_pp, inst)
TEXTLOC *input_doc;
SM_INDEX_TEXTDOC *out_pp;
int inst;
{
    STATIC_INFO *ip;
    SM_INDEX_TEXTDOC orig_pp;

    PRINT_TRACE (2, print_string, "Trace: entering pp_window");
    PRINT_TRACE (6, print_textloc, input_doc);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "pp_window");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (UNDEF == ip->pp.ptab->proc (input_doc, &orig_pp, ip->pp.inst) ||
        UNDEF == pp_pp_window (ip, &orig_pp, out_pp))
        return (UNDEF);

    PRINT_TRACE (4, print_int_textdoc, input_doc);
    PRINT_TRACE (2, print_string, "Trace: leaving pp_window");

    return (1);
}

int
close_pp_window(inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_pp_window");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_pp_window");
        return (UNDEF);
    }

    ip  = &info[inst];
    ip->valid_info--;

    /* Output everything and free buffers if this was last close of this
       inst */
    if (ip->valid_info == 0) {
        (void) free ((char *) ip->sections);
        (void) free ((char *) ip->new_doc_sections);
        (void) free (ip->new_doc_text);
    }

    if (UNDEF == ip->pp.ptab->close_proc (ip->pp.inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_pp_window");
    return (0);
}


static int
pp_pp_window (ip, in_pp, out_pp)
STATIC_INFO *ip;
SM_INDEX_TEXTDOC *in_pp;
SM_INDEX_TEXTDOC *out_pp;
{
    long mem_needed;
    long i;
    char *new_doc_text;
    char *new_doc_text_ptr;
    SM_DISP_SEC *new_doc_sections;
    long num_new_doc_sections;
    long size_section;

    PRINT_TRACE (8, print_string, "in pp_pp_windows 1");
    PRINT_TRACE (8, print_int_textdoc, in_pp);

    out_pp->id_num = in_pp->id_num;
    out_pp->doc_text = in_pp->doc_text;

    out_pp->textloc_doc.id_num = in_pp->textloc_doc.id_num;
    out_pp->textloc_doc.begin_text = in_pp->textloc_doc.begin_text;
    out_pp->textloc_doc.end_text = in_pp->textloc_doc.end_text;
    out_pp->textloc_doc.file_name = in_pp->textloc_doc.file_name;
    out_pp->textloc_doc.title = in_pp->textloc_doc.title;
    out_pp->textloc_doc.doc_type = in_pp->textloc_doc.doc_type;

    out_pp->mem_doc.id_num.id = in_pp->textloc_doc.id_num; 
    EXT_NONE(out_pp->mem_doc.id_num.ext);
    out_pp->mem_doc.file_name = in_pp->mem_doc.file_name;
    out_pp->mem_doc.title = in_pp->mem_doc.title;

    /* Copy document, joining adjacent sections if the same section name */
    ip->num_sections = 0;
    mem_needed = 0;
    for (i = 0; i < in_pp->mem_doc.num_sections; i++) {
        mem_needed += in_pp->mem_doc.sections[i].end_section -
            in_pp->mem_doc.sections[i].begin_section;
    }
    if (mem_needed > ip->max_new_doc_text) {
        (void) free (ip->new_doc_text);
        ip->max_new_doc_text += mem_needed;
        if (NULL == (ip->new_doc_text =
                     malloc ((unsigned) ip->max_new_doc_text)))
            return (UNDEF);
    }
    if (in_pp->mem_doc.num_sections > ip->max_new_doc_sections) {
        (void) free ((char *) ip->new_doc_sections);
        ip->max_new_doc_sections += in_pp->mem_doc.num_sections;
        if (NULL == (ip->new_doc_sections = ((SM_DISP_SEC *)
                                     malloc (ip->max_new_doc_sections *
                                             sizeof (SM_DISP_SEC)))))
        return (UNDEF);
    }
    new_doc_text = ip->new_doc_text;
    new_doc_sections = ip->new_doc_sections;
    num_new_doc_sections = 0;
    new_doc_text_ptr = new_doc_text;
    for (i = 0; i < in_pp->mem_doc.num_sections; i++) {
        size_section = in_pp->mem_doc.sections[i].end_section -
            in_pp->mem_doc.sections[i].begin_section;
        if (size_section == 0)
            continue;
        bcopy (&in_pp->doc_text[in_pp->mem_doc.sections[i].begin_section],
               new_doc_text_ptr,
               size_section);
        if (num_new_doc_sections == 0 ||
            new_doc_sections[num_new_doc_sections-1].section_id !=
            in_pp->mem_doc.sections[i].section_id) {
            new_doc_sections[num_new_doc_sections].section_id = 
                in_pp->mem_doc.sections[i].section_id;
            new_doc_sections[num_new_doc_sections].begin_section =
                new_doc_text_ptr - new_doc_text;
        }
        else {
            num_new_doc_sections--;
        }
        new_doc_text_ptr += size_section;
        new_doc_sections[num_new_doc_sections].end_section = 
            new_doc_text_ptr - new_doc_text;
        num_new_doc_sections++;
    }

    for (i = 0; i < num_new_doc_sections; i++) {
        if (UNDEF == parse_section (ip, 
                                    &new_doc_sections[i],
                                    new_doc_text))
            return (UNDEF);
    }

    out_pp->mem_doc.num_sections = ip->num_sections;
    out_pp->mem_doc.sections = ip->sections;
    out_pp->doc_text = new_doc_text;

    PRINT_TRACE (8, print_string, "in pp_pp_windows 2");
    PRINT_TRACE (8, print_int_textdoc, out_pp);
    return (1);
}


static int
parse_section (ip, section, doc_text)
STATIC_INFO *ip;
SM_DISP_SEC *section;
char *doc_text;
{
    char *end_ptr = &doc_text[section->end_section];
    char *start_ptr = &doc_text[section->begin_section]; /* start of current */
    char *ptr;
    long word_count;
    
    word_count = 0;
    ptr = start_ptr;
    while (ptr < end_ptr) {
        while (ptr < end_ptr && isspace (*ptr))
            ptr++;
        while (ptr < end_ptr && !isspace (*ptr))
            ptr++;
        word_count++;
        if (word_count == window_size) {
            if (UNDEF == new_section (ip,
                                      section->section_id,
                                      start_ptr - doc_text,
                                      ptr - doc_text))
                return (UNDEF);
            word_count = 0;
            start_ptr = ptr;
        }
    }
    if (UNDEF == new_section (ip,
                              section->section_id,
                              start_ptr - doc_text,
                              ptr - doc_text))
        return (UNDEF);

	if (!overlap)
		return 1;

    /* Do it all over again, except skip first window_size/2 words,
       and don't include a final fragment except if > window_size/2 */
    /* Intent is to have this second pass cover any logical paragraph
       that got split by the first pass */
    word_count = 0 - (window_size / 2);
    start_ptr = &doc_text[section->begin_section];
    ptr = start_ptr;
    while (ptr < end_ptr) {
        while (ptr < end_ptr && isspace (*ptr))
            ptr++;
        while (ptr < end_ptr && !isspace (*ptr))
            ptr++;
        word_count++;
        if (word_count == 0)
            start_ptr = ptr;
        if (word_count == window_size) {
            if (UNDEF == new_section (ip,
                                      section->section_id,
                                      start_ptr - doc_text,
                                      ptr - doc_text))
                return (UNDEF);
            word_count = 0;
            start_ptr = ptr;
        }
    }
    if (word_count >= (window_size / 2)) {
        if (UNDEF == new_section (ip,
                                  section->section_id,
                                  start_ptr - doc_text,
                                  ptr - doc_text))
            return (UNDEF);
    }
    return (1);
}


static int
new_section (ip, id, begin_section, end_section)
STATIC_INFO *ip;
char id;
int begin_section;
int end_section;
{
    if (ip->num_sections >= ip->max_sections) {
        ip->max_sections += ip->max_sections;
        if (NULL == (ip->sections = (SM_DISP_SEC *)
                     realloc ((char *) ip->sections,
                              (unsigned) ip->max_sections *
                              sizeof (SM_DISP_SEC))))
            return (UNDEF);
    }

    ip->sections[ip->num_sections].section_id = id;
    ip->sections[ip->num_sections].begin_section = begin_section;
    ip->sections[ip->num_sections].end_section = end_section;
    ip->num_sections++;
    return (1);
}

