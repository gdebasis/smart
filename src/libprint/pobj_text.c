#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/pobj_text.c,v 11.2 1993/02/03 16:54:10 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 print document texts 
 *1 print.obj.doctext
 *2 print_obj_text (in_file, out_file, inst)
 *3   char *in_file;
 *3   char *out_file;
 *3   int inst;
 *4 init_print_obj_doctext (spec, unused)
 *5   "print.doc.indivtext"
 *5   "print.doc.textloc_file"
 *5   "print.doc.textloc_file.rmode"
 *5   "print.trace"
 *4 close_print_obj_text (inst)
 *6 global_start,global_end used to indicate what range of docs will be printed
 *7 The textloc relation "in_file" (if not VALID_FILE, then use textloc_file),
 *7 will be used to print all doc texts in that file (modulo global_start,
 *7 global_end).  Text output to go into file "out_file" (if not VALID_FILE,
 *7 then stdout).
 *8 Procedure indivtext gives format of doc text output.
***********************************************************************/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 print query texts 
 *1 print.mid.querytext
 *2 print_obj_text (in_file, out_file, inst)
 *3   char *in_file;
 *3   char *out_file;
 *3   int inst;
 *4 init_print_querytext (spec, unused)
 *5   "print.query.indivtext"
 *5   "print.query.textloc_file"
 *5   "print.query.textloc_file.rmode"
 *5   "print.trace"
 *4 close_print_obj_text (inst)
 *6 global_start,global_end used to indicate range of queries to be printed
 *7 The textloc relation "in_file" (if not VALID_FILE, then use textloc_file),
 *7 will be used to print all query texts in that file (modulo global_start,
 *7 global_end).  Text output to go into file "out_file" (if not VALID_FILE,
 *7 then stdout).
 *8 Procedure indivtext gives format of query text output.
***********************************************************************/

#include <fcntl.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "textloc.h"
#include "buf.h"
#include "eid.h"

static PROC_INST indivtext;
static char *textloc_file;
static long textloc_mode;

static SPEC_PARAM doc_spec_args[] = {
    {"print.doc.indivtext",     getspec_func,   (char *) &indivtext.ptab},
    {"print.doc.textloc_file",     getspec_dbfile, (char *) &textloc_file},
    {"print.doc.textloc_file.rmode",getspec_filemode, (char *) &textloc_mode},
    TRACE_PARAM ("print.trace")
    };
static int num_doc_spec_args = sizeof (doc_spec_args) /
         sizeof (doc_spec_args[0]);

static SPEC_PARAM query_spec_args[] = {
    {"print.query.indivtext",     getspec_func,   (char *) &indivtext.ptab},
    {"print.query.textloc_file",     getspec_dbfile, (char *) &textloc_file},
    {"print.query.textloc_file.rmode",getspec_filemode, (char *) &textloc_mode},
    TRACE_PARAM ("print.trace")
    };

static int num_query_spec_args = sizeof (query_spec_args) /
         sizeof (query_spec_args[0]);

static int textloc_index;              /* file descriptor for textloc_file */

static int init_print_obj_text();

int
init_print_obj_doctext (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec,
                              &doc_spec_args[0],
                              num_doc_spec_args)) {
        return (UNDEF);
    }
    if (UNDEF == init_print_obj_text (spec, "doc."))
        return (UNDEF);
    return (0);
}

int
init_print_obj_querytext (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec,
                              &query_spec_args[0],
                              num_query_spec_args)) {
        return (UNDEF);
    }
    if (UNDEF == init_print_obj_text (spec, "query."))
        return (UNDEF);
    return (0);
}

static int
init_print_obj_text (spec, prefix)
SPEC *spec;
char *prefix;
{

    PRINT_TRACE (2, print_string, "Trace: entering init_print_obj_text");

    /* Call all initialization procedures */
    if (UNDEF == (indivtext.inst = indivtext.ptab->init_proc (spec, prefix)))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_print_obj_text");
    return (0);
}

int
print_obj_text (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
    TEXTLOC textloc;
    int status;
    char *final_textloc_file;
    FILE *output;
    SM_BUF output_buf;
    EID curr_doc;

    PRINT_TRACE (2, print_string, "Trace: entering print_obj_text");

    final_textloc_file = VALID_FILE (in_file) ? in_file : textloc_file;
    output = VALID_FILE (out_file) ? fopen (out_file, "w") : stdout;
    if (NULL == output)
        return (UNDEF);
    output_buf.size = 0;

    if (UNDEF == (textloc_index = open_textloc (final_textloc_file,
                                                textloc_mode)))
        return (UNDEF);

    /* Get each document in turn */
    if (global_start > 0) {
        textloc.id_num = global_start;
        if (UNDEF == seek_textloc (textloc_index, &textloc)) {
            return (UNDEF);
        }
    }

    while (1 == (status = read_textloc (textloc_index, &textloc)) &&
           textloc.id_num <= global_end) {
	curr_doc.id = textloc.id_num; EXT_NONE(curr_doc.ext);
        output_buf.end = 0;
        if (1 != indivtext.ptab->proc (&curr_doc, &output_buf, indivtext.inst))
            return (UNDEF);
        (void) fwrite (output_buf.buf, 1, output_buf.end, output);
    }

    if (UNDEF == close_textloc (textloc_index))
        return (UNDEF);
    if (output != stdin)
        (void) fclose (output);

    PRINT_TRACE (2, print_string, "Trace: leaving print_obj_text");
    return (status);
}


int
close_print_obj_text (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_print_obj_text");

    if (UNDEF == indivtext.ptab->close_proc (indivtext.inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_print_obj_text");
    return (0);
}
