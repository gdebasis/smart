#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/add_textloc.c,v 11.2 1993/02/03 16:50:50 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Add location of doc to the collection textloc file
 *1 index.addtextloc.add_textloc
 *2 add_textloc (doc, unused, inst)
 *3   SM_INDEX_TEXTDOC *doc;
 *3   char *unused;
 *3   int inst;
 *4 init_add_textloc (SPEC *spec, char *unused)
 *5   "index.addtextloc.trace"
 *5   "index.doc.textloc_file"
 *5   "index.doc.textloc_file.rwmode"
 *5   "index.title_section"
 *5   "index.max_title_len"
 *5   "index.query.textloc_file"
 *5   "index.query.textloc_file.rwmode"
 *4 close_add_textloc (inst)
***********************************************************************/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "sm_display.h"
#include "docindex.h"
#include "trace.h"
#include "textloc.h"
#include "context.h"
#include "io.h"

static char *textloc_file;
static long textloc_mode;
static long title_section;
static long max_title_len;

static SPEC_PARAM spec_args[] = {
    {"index.doc.textloc_file",  getspec_dbfile,   (char *) &textloc_file},
    {"index.doc.textloc_file.rwmode",  getspec_filemode, (char *) &textloc_mode},
    {"index.title_section",     getspec_long,     (char *) &title_section},
    {"index.max_title_len",     getspec_long,     (char *) &max_title_len},
    TRACE_PARAM ("index.addtextloc.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static SPEC_PARAM qspec_args[] = {
    {"index.query.textloc_file",  getspec_dbfile,   (char *) &textloc_file},
    {"index.query.textloc_file.rwmode",getspec_filemode, (char *) &textloc_mode},
    {"index.title_section",      getspec_long,     (char *) &title_section},
    {"index.max_title_len",     getspec_long,     (char *) &max_title_len},
    TRACE_PARAM ("index.addtextloc.trace")
    };
static int num_qspec_args = sizeof (qspec_args) / sizeof (qspec_args[0]);


static int textloc_index;            /* file descriptor for textloc_file */

static int sectid_num_inst;

static TEXTLOC textloc;

static int no_textloc_flag = 0;      /* Flag indicating if textloc is wanted */

int
init_add_textloc (spec, unused)
SPEC *spec;
char *unused;
{
    /* Lookup the values of the relevant parameters */
    if (check_context (CTXT_INDEXING_DOC)) {
        if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args)) {
            return (UNDEF);
        }
    }
    else if (check_context (CTXT_INDEXING_QUERY)) {
        if (UNDEF == lookup_spec (spec,
                                  &qspec_args[0],
                                  num_qspec_args)) {
            return (UNDEF);
        }
    }
    else {
        set_error (SM_ILLPA_ERR, "Illegal context", "init_add_textloc");
        return (UNDEF);
    }



    PRINT_TRACE (2, print_string, "Trace: entering init_add_textloc");

    /* No textloc file is wanted */
    if (! VALID_FILE (textloc_file)) {
        no_textloc_flag = 1;
        PRINT_TRACE (2, print_string, "Trace: leaving init_add_textloc");
        return (0);
    }

    if (UNDEF == (sectid_num_inst = init_sectid_num (spec, (char *) NULL)))
        return (UNDEF);

    if (UNDEF == (textloc_index = open_textloc (textloc_file,
                                             textloc_mode))) {
        clr_err();
        if (UNDEF == (textloc_index = open_textloc (textloc_file,
                                                 textloc_mode | SCREATE)))
            return (UNDEF);
    }

    if (NULL == (textloc.title = malloc ((unsigned) max_title_len)))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_add_textloc");

    return (0);
}

int
add_textloc (doc, unused, inst)
SM_INDEX_TEXTDOC *doc;
char *unused;
int inst;
{
    long i;
    char *title_ptr = textloc.title;
    char *sec_ptr, *end_ptr;
    long sect_num;

    PRINT_TRACE (2, print_string, "Trace: entering add_textloc");
    PRINT_TRACE (6, print_int_textdoc, doc);

    /* No textloc file is wanted */
    if (no_textloc_flag) {
        PRINT_TRACE (2, print_string, "Trace: leaving add_textloc");
        return (0);
    }

    textloc.begin_text = doc->textloc_doc.begin_text;
    textloc.end_text = doc->textloc_doc.end_text;
    textloc.id_num = doc->textloc_doc.id_num;
    textloc.doc_type = doc->textloc_doc.doc_type;
    textloc.file_name = doc->textloc_doc.file_name;

    /* Add text of title */
    for (i = 0; i < doc->mem_doc.num_sections; i++) {
        if (UNDEF == sectid_num (&doc->mem_doc.sections[i].section_id,
                                 &sect_num,
                                 sectid_num_inst))
            return (UNDEF);
        if (title_section == sect_num) {
            sec_ptr = doc->doc_text + doc->mem_doc.sections[i].begin_section;
            end_ptr = doc->doc_text + doc->mem_doc.sections[i].end_section;
            while (sec_ptr < end_ptr &&
                   title_ptr < &textloc.title[max_title_len-1]) {
                if (isspace (*sec_ptr)) {
                    if (title_ptr != textloc.title && *(title_ptr-1) != ' ') {
                        *title_ptr++ = ' ';
                    }
                }
                else 
                    *title_ptr++ = *sec_ptr;
                sec_ptr++;
            }
        }
    }
    *title_ptr = '\0';

    if ( UNDEF == seek_textloc  (textloc_index, &textloc) ||
        1 != write_textloc (textloc_index, &textloc)) {
        return (UNDEF);
    }

    PRINT_TRACE (4, print_textloc, &textloc);
    PRINT_TRACE (2, print_string, "Trace: leaving add_textloc");

    return (1);
}


int
close_add_textloc(inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_add_textloc");

    /* No textloc file is wanted */
    if (no_textloc_flag) {
        PRINT_TRACE (2, print_string, "Trace: leaving close_add_textloc");
        return (0);
    }

    if (UNDEF == close_sectid_num (sectid_num_inst))
        return (UNDEF);

    if (UNDEF == close_textloc (textloc_index))
        return (UNDEF);

    (void) free(textloc.title);

    PRINT_TRACE (2, print_string, "Trace: leaving close_add_textloc");

    return (1);
}
