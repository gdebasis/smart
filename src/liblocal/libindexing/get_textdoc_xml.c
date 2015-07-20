#ifdef RCSID
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Get the hierarchical structure of the text content of an XML file
 *1 
 *2 get_textdoc_xml (did, textdoc, inst)
 *3   long did;
 *3   SM_INDEX_TEXTDOC *textdoc;
 *3   int inst;
 *4 init_get_textdoc_xml (spec, unused)
 *5   "index.get_textdoc_xml.trace"
 *5   "index.get_textdoc_xml.named_get_text"
 *4 close_get_textdoc_xml (inst)

 *7 Generates the entire SM_INDEX_TEXTDOC structure for an XML document.
 *7 1. get text by calling named_get_text
 *7 2. build XML tree (as in pp_xml.c)
 *7 3. traverse tree and put text of interest in textdoc->doc_text
 *7 4. put hierarchical structure of text in sections / sub-sections / 
 *7    sub-sub-sections / ... of textdoc->mem_doc
 *7 Return UNDEF if error, else 0;

 *8 See pp_xml.c (a substantial part of the code is borrowed from there).

 *9 Intended for use during reranking. Thus, we assume that only a single 
 *9 instance will be required.
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
#include <libxml/parser.h>
#include <libxml/tree.h>

static PROC_TAB *get_text;
static int get_text_inst;
static SPEC_PARAM spec_args[] = {
    {"index.get_textdoc_xml.named_get_text", getspec_func, (char *) &get_text},
    TRACE_PARAM ("index.get_textdoc_xml.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int traverse_tree();


int
init_get_textdoc_xml (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
	return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_get_textdoc_xml");

    if (UNDEF == (get_text_inst = get_text->init_proc(spec, "doc.")))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_get_textdoc_xml");

    return new_inst;
}


int
get_textdoc_xml (did, textdoc, inst)
EID did;
SM_INDEX_TEXTDOC *textdoc;
int inst;
{
    int status;
    long i, j, k;
    xmlDoc *doc = NULL;
    xmlNode *root_element = NULL;

    PRINT_TRACE (2, print_string, "Trace: entering get_textdoc_xml");

    status = get_text->proc(did, textdoc, get_text_inst);
    if (UNDEF == status)
        return (UNDEF);
    else if (0 == status) {
        PRINT_TRACE (2, print_string, "Trace: leaving pp_named_doc");
        return (0);
    }

    /* copy from TEXTLOC to other output places */
    output_doc->id_num.id = output_doc->textloc_doc.id_num;
    EXT_NONE(output_doc->id_num.ext);
    output_doc->mem_doc.id_num.id = output_doc->textloc_doc.id_num;
    EXT_NONE(output_doc->mem_doc.id_num.ext);
    output_doc->mem_doc.file_name = output_doc->textloc_doc.file_name;
    output_doc->mem_doc.title = output_doc->textloc_doc.title;

    /* parse document */
    if (NULL == (doc = xmlReadMemory(output_doc->doc_text, //char *
                                     output_doc->textloc_doc.end_text -
                                     output_doc->textloc_doc.begin_text, //size
                                     NULL, NULL,
                                     XML_PARSE_RECOVER|XML_PARSE_PEDANTIC))) {
        set_error(errno, "parsing XML document", "pp_xml");
        return UNDEF;
    }
    root_element = xmlDocGetRootElement(doc);

    if (UNDEF == calculate_size(root_element))
        return UNDEF;

    xmlFreeDoc(doc);

    PRINT_TRACE (2, print_string, "Trace: leaving get_textdoc_xml");
    return 0;
}


int
close_get_textdoc_xml (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_get_textdoc_xml");

    if (UNDEF == get_text->close_proc (get_text_inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_get_textdoc_xml");
    return (0);
}


static int
calculate_size(a_node)
xmlNode *a_node;
{
    xmlNode *cur_node = NULL;
    static int i = 0;

    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_TEXT_NODE) {
            if (lookup_section_start(cur_node->parent->name, 
                                     strlen(cur_node->parent->name),  
                                     &new_secptr, ip)) {

                ip->curr_location += strlen(cur_node->content);
            }
            /*else fprintf(stderr, "Not indexed: ");
	     *fprintf(stderr, "%s\n%s\n\n", 
	     *     cur_node->parent->name,
	     *     cur_node->content);*/
	     }
        traverse_tree(cur_node->children, buf, ip); 
    }
    return 1;
}
