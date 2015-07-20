#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/tloc_tloc_o.c,v 11.2 1993/02/03 16:49:43 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Copy a textloc relational object
 *1 convert.obj.textloc_textloc
 *2 textloc_textloc_obj (in_textloc_file, out_textloc_file, inst)
 *3   char *in_textloc_file;
 *3   char *out_textloc_file;
 *3   int inst;

 *4 init_textloc_textloc_obj (spec, unused)
 *5   "convert.textloc_textloc.textloc_file"
 *5   "convert.textloc_textloc.textloc_file.rmode"
 *5   "convert.textloc_textloc.textloc_file.rwcmode"
 *5   "convert.deleted_doc_file"
 *5   "convert.include_doc_file"
 *5   "textloc_textloc_obj.trace"
 *4 close_textloc_textloc_obj(inst)

 *6 global_start and global_end are checked.  Only those textlocs with
 *6 id_num falling between them are converted.
 *7 Read textloc tuples from in_textloc_file and copy to out_textloc_file.
 *7 If in_textloc_file is NULL then textloc_file is used.
 *7 If out_textloc_file is NULL, then error.
 *7
 *7 If deleted_doc_file is a valid file, then it is assumed to be a
 *7 text file composed of docid's to be deleted during the copying process.
 *7 If 'include_doc_file" is valid, then it is assumed to be a text
 *7 file composed of docids to be included.  If BOTH deleted_doc_file
 *7 and 'include_doc_file' are valid, only items in "include" but not
 *7 in "deleted" are added.
 *7
 *7 The docid's are assumed to be numerically sorted.
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
#include "textloc.h"
#include "io.h"

static char *default_textloc_file;
static long read_mode;
static long write_mode;
static char *deleted_doc_file;
static char *include_doc_file;

static SPEC_PARAM spec_args[] = {
    {"convert.textloc_textloc.textloc_file",
                  getspec_dbfile, (char *) &default_textloc_file},
    {"convert.textloc_textloc.textloc_file.rmode",
                  getspec_filemode, (char *) &read_mode},
    {"convert.textloc_textloc.textloc_file.rwcmode",
                  getspec_filemode, (char *) &write_mode},
    {"convert.deleted_doc_file",  getspec_dbfile, (char *) &deleted_doc_file},
    {"convert.include_doc_file",  getspec_dbfile, (char *) &include_doc_file},
    TRACE_PARAM ("textloc_textloc_obj.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

int
init_textloc_textloc_obj (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_textloc_textloc_obj");

    PRINT_TRACE (2, print_string, "Trace: leaving init_textloc_textloc_obj");

    return (0);
}

int
textloc_textloc_obj (in_textloc_file, out_textloc_file, inst)
char *in_textloc_file;
char *out_textloc_file;
int inst;
{
    TEXTLOC textloc;
    int status;
    int in_index, out_index;
    long *del_dids;
    long num_del_dids;
    long *inc_dids;
    long num_inc_dids;
    long del_index, inc_index;

    PRINT_TRACE (2, print_string, "Trace: entering textloc_textloc_obj");

    num_del_dids = 0;
    if (VALID_FILE (deleted_doc_file) &&
        UNDEF == text_long_array(deleted_doc_file, &del_dids, &num_del_dids))
        return (UNDEF);

    num_inc_dids = 0;
    if (VALID_FILE (include_doc_file) &&
        UNDEF == text_long_array(include_doc_file, &inc_dids, &num_inc_dids))
        return (UNDEF);

    if (in_textloc_file == NULL)
        in_textloc_file = default_textloc_file;
    if (UNDEF == (in_index = open_textloc (in_textloc_file, read_mode))){
        return (UNDEF);
    }

    if (! VALID_FILE (out_textloc_file)) {
        set_error (SM_ILLPA_ERR, "textloc_textloc_obj", out_textloc_file);
        return (UNDEF);
    }
    if (UNDEF == (out_index = open_textloc (out_textloc_file, write_mode))){
        return (UNDEF);
    }

    /* Copy each textloc in turn */
    textloc.id_num = 0;
    if (global_start > 0) {
        textloc.id_num = global_start;
        if (UNDEF == seek_textloc (in_index, &textloc)) {
            return (UNDEF);
        }
    }

    del_index = inc_index = 0;
    while (1 == (status = read_textloc (in_index, &textloc)) &&
           textloc.id_num <= global_end) {
        /* See if document should be deleted */
        while (del_index < num_del_dids &&
               del_dids[del_index] < textloc.id_num)
            del_index++;
        if (del_index < num_del_dids &&
            del_dids[del_index] == textloc.id_num)
            continue;

	/* Get to proper spot in include list */
	while (inc_index < num_inc_dids &&
	       inc_dids[inc_index] < textloc.id_num)
	    inc_index++;
	/* Include if no include list or this one's in it */
	if (num_inc_dids == 0 ||
	    (inc_index < num_inc_dids &&
	     inc_dids[inc_index] == textloc.id_num))

	    if (UNDEF == seek_textloc (out_index, &textloc) ||
		1 != write_textloc (out_index, &textloc))
		return (UNDEF);
    }

    if (UNDEF == close_textloc (out_index) || 
        UNDEF == close_textloc (in_index))
        return (UNDEF);
    if (VALID_FILE (deleted_doc_file) &&
        UNDEF == free_long_array (deleted_doc_file, &del_dids, &num_del_dids))
        return (UNDEF);
    if (VALID_FILE (include_doc_file) &&
        UNDEF == free_long_array (include_doc_file, &inc_dids, &num_inc_dids))
        return (UNDEF);
    
    PRINT_TRACE (2, print_string, "Trace: leaving print_textloc_textloc_obj");
    return (status);
}

int
close_textloc_textloc_obj(inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_textloc_textloc_obj");

    PRINT_TRACE (2, print_string, "Trace: entering close_textloc_textloc_obj");
    return (0);
}
