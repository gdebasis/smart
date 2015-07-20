#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/pobj_rel_hd.c,v 11.2 1993/02/03 16:54:15 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 print the releation header of an arbitrary SMART relational object
 *1 print.obj.rel_header
 *2 print_obj_rel_header (in_file, out_file, inst)
 *3   char *in_file;
 *3   char *out_file;
 *3   int inst;
 *4 init_print_obj_rel_header (spec, unused)
 *5   "print.trace"
 *4 close_print_obj_rel_header (inst)
 *6 global_start,global_end used to indicate what range of cons will be printed
 *7 The header to relational object "in_file" (if not VALID_FILE, then error)
 *7 is printed.  Rel_header output to go into file "out_file" (if not VALID_FILE,
 *7 then stdout).
***********************************************************************/

#include <fcntl.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "rel_header.h"
#include "buf.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("print.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);


int
init_print_obj_rel_header (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_print_obj_rel_header");

    PRINT_TRACE (2, print_string, "Trace: leaving init_print_obj_rel_header");
    return (0);
}

int
print_obj_rel_header (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
    REL_HEADER *rh;
    FILE *output;
    SM_BUF output_buf;

    PRINT_TRACE (2, print_string, "Trace: entering print_obj_rel_header");

    if (! VALID_FILE (in_file)) {
        set_error (SM_ILLPA_ERR, "print_obj_rel_header", in_file);
        return (UNDEF);
    }
    output = VALID_FILE (out_file) ? fopen (out_file, "w") : stdout;
    if (NULL == output)
        return (UNDEF);
    output_buf.size = 0;

    if (NULL == (rh = get_rel_header (in_file)))
        return (UNDEF);

    print_rel_header (rh, &output_buf);
    (void) fwrite (output_buf.buf, 1, output_buf.end, output);

    if (output != stdin)
        (void) fclose (output);

    PRINT_TRACE (2, print_string, "Trace: leaving print_obj_rel_header");
    return (0);
}


int
close_print_obj_rel_header (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_print_obj_rel_header");

    PRINT_TRACE (2, print_string, "Trace: leaving close_print_obj_rel_header");
    return (0);
}
