#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/pobj_did_nv.c,v 11.2 1993/02/03 16:54:22 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 print textloc object
 *1 print.obj.did_nonvalid
 *2 print_obj_did_nonvalid (in_file, out_file, inst)
 *3   char *in_file;
 *3   char *out_file;
 *3   int inst;
 *4 init_print_obj_did_nonvalid (spec, unused)
 *5   "print.textloc_file"
 *5   "print.textloc_file.rmode"
 *5   "print.trace"
 *4 close_print_obj_did_nonvalid (inst)
 *6 global_start,global_end used to indicate what range of dids will be printed
 *7 The textloc relation "in_file" (if not VALID_FILE, then use text_loc_file),
 *7 is examined tuple by tuple.  If a tuple has a non-valid filename (file
 *7 does not exist), then the corresponding did is printed.
 *7 Output to go into file "out_file" (if not VALID_FILE, then stdout).
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "textloc.h"

static char *textloc_file;
static long textloc_mode;

static SPEC_PARAM spec_args[] = {
    {"print.doc.textloc_file",     getspec_dbfile, (char *) &textloc_file},
    {"print.doc.textloc_file.rmode",getspec_filemode, (char *) &textloc_mode},
    TRACE_PARAM ("print.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);


int
init_print_obj_did_nonvalid (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_print_obj_did_nonvalid");

    PRINT_TRACE (2, print_string, "Trace: leaving init_print_obj_did_nonvalid");
    return (0);
}

int
print_obj_did_nonvalid (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
    TEXTLOC textloc;
    int status;
    char *final_textloc_file;
    FILE *output;
    int textloc_index;              /* file descriptor for textloc_file */

    PRINT_TRACE (2, print_string, "Trace: entering print_obj_did_nonvalid");

    final_textloc_file = VALID_FILE (in_file) ? in_file : textloc_file;
    output = VALID_FILE (out_file) ? fopen (out_file, "w") : stdout;
    if (NULL == output)
        return (UNDEF);

    if (UNDEF == (textloc_index = open_textloc (final_textloc_file, textloc_mode)))
        return (UNDEF);

    /* Get each text_loc in turn */
    if (global_start > 0) {
        textloc.id_num = global_start;
        if (UNDEF == seek_textloc (textloc_index, &textloc)) {
            return (UNDEF);
        }
    }

    while (1 == (status = read_textloc (textloc_index, &textloc)) &&
           textloc.id_num <= global_end) {
        if (! unix_file_exists (textloc.file_name))
            (void) fprintf (output, "%ld\n", textloc.id_num);
    }

    if (UNDEF == close_textloc (textloc_index))
        return (UNDEF);

    if (output != stdin)
        (void) fclose (output);

    PRINT_TRACE (2, print_string, "Trace: leaving print_obj_did_nonvalid");
    return (status);
}


int
close_print_obj_did_nonvalid (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_print_obj_did_nonvalid");

    PRINT_TRACE (2, print_string, "Trace: leaving close_print_obj_did_nonvalid");
    return (0);
}
