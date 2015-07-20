#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/did_phr.c,v 11.2 1993/02/03 16:49:43 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Print document/query texts.
 *1 local.print.obj.doctext
 *1 local.print.obj.querytext
 *2 l_print_obj_text (unused1, unused2, inst)
 *3   char *unused1, *unused2;
 *3   int inst;

 *4 init_l_print_obj_doctext (spec, unused)
 *4 init_l_print_obj_querytext (spec, unused)
 *5   "print.textloc_file"
 *5   "print.doc_list_file"
 *5   "print.indivtext"
 *5   "print.temp_dir"
 *5   "print.trace"
 *4 close_l_print_obj_text (inst)

 *7 Prints the texts of the documents listed in doc_list_file to individual
 *7 files in temp_dir.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "proc.h"
#include "textloc.h"
#include "sm_display.h"
#include "docindex.h"
#include "inst.h"
#include "vector.h"
#include "io.h"

static char *textloc_file, *doc_list_file, *temp_dir;
static PROC_TAB *print;
static SPEC_PARAM spec_args[] = {
    {"print.textloc_file", getspec_dbfile, (char *)&textloc_file},
    {"print.doc_list_file", getspec_dbfile, (char *)&doc_list_file},
    {"print.indivtext", getspec_func, (char *)&print},
    {"print.temp_dir", getspec_dbfile, (char *)&temp_dir},
    TRACE_PARAM ("print.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int p_inst;
static SM_BUF output_buf = {0, 0, (char *) 0};

int
init_l_print_obj_doctext (spec, unused)
SPEC *spec;
char *unused;
{
    PRINT_TRACE (2, print_string, "Trace: entering init_l_print_obj_text");

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
	return (UNDEF);

    if (UNDEF == (p_inst = print->init_proc(spec, "doc.")))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_l_print_obj_text");

    return(0);
}

int
init_l_print_obj_querytext (spec, unused)
SPEC *spec;
char *unused;
{
    PRINT_TRACE (2, print_string, "Trace: entering init_l_print_obj_text");

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
	return (UNDEF);

    if (UNDEF == (p_inst = print->init_proc(spec, "query.")))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_l_print_obj_text");

    return(0);
}

int
l_print_obj_text (unused1, unused2, inst)
char *unused1, *unused2;
int inst;
{
    char tmp_file[PATH_LEN];
    long tloc_fd, status;
    long *doclist, num_docs, curr_doc;
    FILE *fp;
    EID curr;
    TEXTLOC textloc;

    PRINT_TRACE (2, print_string, "Trace: entering l_print_obj_text");

    if (UNDEF == (tloc_fd = open_textloc (textloc_file, SRDONLY)))
	return UNDEF;
    num_docs = 0;
    if (VALID_FILE (doc_list_file) &&
        UNDEF == text_long_array(doc_list_file, &doclist, &num_docs))
        return (UNDEF);

    /* Get textloc */
    textloc.id_num = 0;
    if (global_start > 0) {
        textloc.id_num = global_start;
        if (UNDEF == seek_textloc (tloc_fd, &textloc)) {
            return (UNDEF);
        }
    }

    curr_doc = 0;
    while (1 == (status = read_textloc (tloc_fd, &textloc)) &&
           textloc.id_num <= global_end) {
	while (curr_doc < num_docs &&
	       doclist[curr_doc] < textloc.id_num)
	    curr_doc++;
	if (num_docs == 0 ||
	    (curr_doc < num_docs &&
	     doclist[curr_doc] == textloc.id_num)) {
	    curr.id = textloc.id_num; EXT_NONE(curr.ext);
	    /* Print text to a tmp file */
	    output_buf.end = 0;	
	    if (UNDEF == print->proc (&curr, &output_buf, p_inst))
		return UNDEF;
	    sprintf (tmp_file, "%s/sm_text.%ld", temp_dir, curr.id);
	    if (NULL == (fp = fopen(tmp_file, "w"))) {
		set_error (0, "Opening temp file", "print_obj_text");
		return(UNDEF);
	    }
	    if (output_buf.end != fwrite(output_buf.buf, 1, output_buf.end, fp)) {
		set_error (0, "Writing temp file", "print_obj_text");
		return(UNDEF);
	    }
	    fclose(fp);
	}
    }

    if (UNDEF == close_textloc(tloc_fd))
	return(UNDEF);
    if (VALID_FILE (doc_list_file) &&
        UNDEF == free_long_array (doc_list_file, &doclist, &num_docs))
        return (UNDEF);
 
    PRINT_TRACE (2, print_string, "Trace: leaving l_print_obj_text");
    return 0;
}

int
close_l_print_obj_text (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_l_print_obj_text");

    if (UNDEF == print->close_proc(p_inst))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_l_print_obj_text");
    return (0);
}
