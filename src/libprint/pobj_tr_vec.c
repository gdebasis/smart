#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/pobj_tr_vec.c,v 11.2 1993/02/03 16:54:11 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 print tr_vec object
 *1 print.obj.tr_vec
 *2 print_obj_tr_vec (in_file, out_file, inst)
 *3   char *in_file;
 *3   char *out_file;
 *3   int inst;
 *4 init_print_obj_tr_vec (spec, unused)
 *5   "print.tr_file"
 *5   "print.tr_file.rmode"
 *5   "print.trace"
 *4 close_print_obj_tr_vec (inst)
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

static char *tr_vec_file;
static long tr_vec_mode;

static SPEC_PARAM spec_args[] = {
    {"print.tr_file",     getspec_dbfile, (char *) &tr_vec_file},
    {"print.tr_file.rmode",getspec_filemode, (char *) &tr_vec_mode},
    TRACE_PARAM ("print.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);


int
init_print_obj_tr_vec (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_print_obj_tr_vec");

    PRINT_TRACE (2, print_string, "Trace: leaving init_print_obj_tr_vec");
    return (0);
}

int
print_obj_tr_vec (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
    TR_VEC tr_vec;
    int status;
    char *final_tr_vec_file;
    FILE *output;
    SM_BUF output_buf;
    int tr_vec_index;              /* file descriptor for tr_vec_file */

    PRINT_TRACE (2, print_string, "Trace: entering print_obj_tr_vec");

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

    while (1 == (status = read_tr_vec (tr_vec_index, &tr_vec)) &&
           tr_vec.qid <= global_end) {
        output_buf.end = 0;
        print_tr_vec (&tr_vec, &output_buf);
        (void) fwrite (output_buf.buf, 1, output_buf.end, output);
    }

    if (UNDEF == close_tr_vec (tr_vec_index))
        return (UNDEF);

    if (output != stdin)
        (void) fclose (output);

    PRINT_TRACE (2, print_string, "Trace: leaving print_obj_tr_vec");
    return (status);
}


int
close_print_obj_tr_vec (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_print_obj_tr_vec");

    PRINT_TRACE (2, print_string, "Trace: leaving close_print_obj_tr_vec");
    return (0);
}
