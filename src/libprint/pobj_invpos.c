#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/pobj_inv.c,v 11.2 1993/02/03 16:54:09 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 print inv object
 *1 print.obj.inv
 *2 print_obj_inv (in_file, out_file, inst)
 *3   char *in_file;
 *3   char *out_file;
 *3   int inst;
 *4 init_print_obj_inv (spec, unused)
 *5   "print.inv_file"
 *5   "print.inv_file.rmode"
 *5   "print.trace"
 *4 close_print_obj_inv (inst)
 *6 global_start,global_end used to indicate what range of cons will be printed
 *7 The inv relation "in_file" (if not VALID_FILE, then use inv_file),
 *7 will be used to print all inv entries in that file (modulo global_start,
 *7 global_end).  Inv output to go into file "out_file" (if not VALID_FILE,
 *7 then stdout).
***********************************************************************/

#include <fcntl.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "invpos.h"
#include "buf.h"

static char *inv_file;
static long inv_mode;

static SPEC_PARAM spec_args[] = {
    {"print.inv_file",     getspec_dbfile, (char *) &inv_file},
    {"print.inv_file.rmode",getspec_filemode, (char *) &inv_mode},
    TRACE_PARAM ("print.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);


int
init_print_obj_invpos (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_print_obj_invpos");

    PRINT_TRACE (2, print_string, "Trace: leaving init_print_obj_invpos");
    return (0);
}

int
print_obj_invpos (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
    INVPOS inv;
    int status;
    char *final_inv_file;
    FILE *output;
    SM_BUF output_buf;
    int inv_index;              /* file descriptor for inv_file */

    PRINT_TRACE (2, print_string, "Trace: entering print_obj_invpos");

    final_inv_file = VALID_FILE (in_file) ? in_file : inv_file;
    output = VALID_FILE (out_file) ? fopen (out_file, "w") : stdout;
    if (NULL == output)
        return (UNDEF);
    output_buf.size = 0;

    if (UNDEF == (inv_index = open_invpos (final_inv_file, inv_mode)))
        return (UNDEF);

    /* Get each inverted list in turn */
    if (global_start > 0) {
        inv.id_num = global_start;
        if (UNDEF == seek_invpos (inv_index, &inv)) {
            return (UNDEF);
        }
    }

    while (1 == (status = read_invpos (inv_index, &inv)) &&
           inv.id_num <= global_end) {
        output_buf.end = 0;
        print_invpos (&inv, &output_buf);
        (void) fwrite (output_buf.buf, 1, output_buf.end, output);
    }

    if (UNDEF == close_invpos (inv_index))
        return (UNDEF);

    if (output != stdin)
        (void) fclose (output);

    PRINT_TRACE (2, print_string, "Trace: leaving print_obj_invpos");
    return (status);
}


int
close_print_obj_invpos (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_print_obj_invpos");

    PRINT_TRACE (2, print_string, "Trace: leaving close_print_obj_invpos");
    return (0);
}
