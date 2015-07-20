#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/pobj_wiki.c,v 11.2 1993/02/03 16:54:11 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 print wiki object
 *1 print.obj.wiki
 *2 print_obj_wiki (in_file, out_file, inst)
 *3   char *in_file;
 *3   char *out_file;
 *3   int inst;
 *4 init_print_obj_wiki (spec, unused)
 *5   "print.tr_file"
 *5   "print.tr_file.rmode"
 *5   "print.trace"
 *4 close_print_obj_wiki (inst)
 *6 global_start,global_end used to indicate what range of cons will be printed
 *7 The tr_vec relation "in_file" (if not VALID_FILE, then use tr_file),
 *7 will be used to print all tr_vec entries in that file (modulo global_start,
 *7 global_end).  Tr_vec output to go into file "out_file" (if not VALID_FILE,
 *7 then stdout).
***********************************************************************/

#include <fcntl.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "tr_vec.h"
#include "buf.h"
#include "textloc.h"

static char *tr_vec_file;
static long tr_vec_mode;
static char* textloc_file;
static int textloc_index;
static int textloc_mode;

static SPEC_PARAM spec_args[] = {
    {"print.tr_file",     getspec_dbfile, (char *) &tr_vec_file},
    {"print.tr_file.rmode",getspec_filemode, (char *) &tr_vec_mode},
    {"print.tr_file.textloc_file",     getspec_dbfile, (char *) &textloc_file},
    {"print.tr_file.textloc_file.rmode",getspec_filemode, (char *) &textloc_mode},
    TRACE_PARAM ("print.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);


int
init_print_obj_wiki (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_print_obj_wiki");

    if (UNDEF == (textloc_index = open_textloc (textloc_file, textloc_mode)))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_print_obj_wiki");
    return (0);
}

int
print_obj_wiki (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
    TR_VEC tr_vec;
    int status;
    char *final_tr_vec_file;
    FILE *output;
    SM_BUF output_buf;
	TR_TUP *tr_tup;
    int tr_vec_index;              /* file descriptor for tr_vec_file */
    char temp_buf[PATH_LEN];
	char* docname;
	TEXTLOC textloc;

    PRINT_TRACE (2, print_string, "Trace: entering print_obj_wiki");

    final_tr_vec_file = VALID_FILE (in_file) ? in_file : tr_vec_file;
    output = VALID_FILE (out_file) ? fopen (out_file, "w") : stdout;
    if (NULL == output)
        return (UNDEF);
    output_buf.size = 0;

    if (UNDEF == (tr_vec_index = open_tr_vec (final_tr_vec_file, tr_vec_mode)))
        return (UNDEF);

    /* Get each tr_vec in turn */
    if (global_start > 0) {
        tr_vec.qid = global_start;
        if (UNDEF == seek_tr_vec (tr_vec_index, &tr_vec)) {
            return (UNDEF);
        }
    }

    while (1 == (status = read_tr_vec (tr_vec_index, &tr_vec))) {
        output_buf.end = 0;

        for (tr_tup = tr_vec.tr; tr_tup < &tr_vec.tr[tr_vec.num_tr]; tr_tup++) {
            textloc.id_num = tr_tup->did;
            if (UNDEF == seek_textloc (textloc_index, &textloc) ||
                UNDEF == read_textloc (textloc_index, &textloc))
                return UNDEF;
            docname = textloc.file_name;

	        snprintf(temp_buf, sizeof(temp_buf), "%d\t%s\n", tr_tup->rank, docname);
        	if (UNDEF == add_buf_string (temp_buf, &output_buf))
            	return UNDEF;
        }
       	fwrite(output_buf.buf, 1, output_buf.end, output);
    }
		
    if (UNDEF == close_tr_vec (tr_vec_index))
        return (UNDEF);

    if (output != stdin)
        (void) fclose (output);

    PRINT_TRACE (2, print_string, "Trace: leaving print_obj_wiki");
    return (status);
}


int
close_print_obj_wiki (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_print_obj_wiki");

    if (UNDEF == close_textloc (textloc_index))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_print_obj_wiki");
    return (0);
}
