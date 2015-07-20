#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/pobj_fdbk_stats.c,v 11.0 1992/07/21 18:23:30 chrisb Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 print fdbk_stats object
 *1 print.obj.fdbk_stats
 *2 print_obj_fdbk_stats (in_file, out_file, inst)
 *3   char *in_file;
 *3   char *out_file;
 *3   int inst;
 *4 init_print_obj_fdbk_stats (spec, unused)
 *5   "print.fdbk_stats_file"
 *5   "print.fdbk_stats_file.rmode"
 *5   "print.trace"
 *4 close_print_obj_fdbk_stats (inst)
 *6 global_start,global_end used to indicate what range of cons will be printed
 *7 The fdbk_stats relation "in_file" (if not VALID_FILE, then use 
 *7 fdbk_stats_file), will be used to print all fdbk_stats entries 
 *7 in that file (modulo global_start, global_end).  
 *7 Fdbk_stats output to go into file "out_file" (if not VALID_FILE,
 *7 then stdout).
***********************************************************************/

#include <fcntl.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "fdbk_stats.h"
#include "buf.h"

static char *fdbk_stats_file;
static long fdbk_stats_mode;

static SPEC_PARAM spec_args[] = {
    {"print.fdbk_stats_file",     getspec_dbfile, (char *) &fdbk_stats_file},
    {"print.fdbk_stats_file.rmode",getspec_filemode, (char *) &fdbk_stats_mode},
    TRACE_PARAM ("print.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

int print_fdbk_stats();

int
init_print_obj_fdbk_stats (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_print_obj_fdbk_stats");

    PRINT_TRACE (2, print_string, "Trace: leaving init_print_obj_fdbk_stats");
    return (0);
}

int
print_obj_fdbk_stats (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
    FDBK_STATS fdbk_stats;
    int status;
    char *final_fdbk_stats_file;
    FILE *output;
    SM_BUF output_buf;
    int fdbk_stats_index;             /* file descriptor for fdbk_stats_file */

    PRINT_TRACE (2, print_string, "Trace: entering print_obj_fdbk_stats");

    final_fdbk_stats_file = VALID_FILE (in_file) ? in_file : fdbk_stats_file;
    output = VALID_FILE (out_file) ? fopen (out_file, "w") : stdout;
    if (NULL == output)
        return (UNDEF);
    output_buf.size = 0;

    if (UNDEF == (fdbk_stats_index = open_fdbkstats (final_fdbk_stats_file,
                                                     fdbk_stats_mode)))
        return (UNDEF);

    /* Get each fdbk_stats in turn */
    if (global_start > 0) {
        fdbk_stats.id_num = global_start;
        if (UNDEF == seek_fdbkstats (fdbk_stats_index, &fdbk_stats)) {
            return (UNDEF);
        }
    }

    while (1 == (status = read_fdbkstats (fdbk_stats_index, &fdbk_stats)) &&
           fdbk_stats.id_num <= global_end) {
        output_buf.end = 0;
        print_fdbk_stats (&fdbk_stats, &output_buf);
        (void) fwrite (output_buf.buf, 1, output_buf.end, output);
    }

    if (UNDEF == close_fdbkstats (fdbk_stats_index))
        return (UNDEF);

    if (output != stdin)
        (void) fclose (output);

    PRINT_TRACE (2, print_string, "Trace: leaving print_obj_fdbk_stats");
    return (status);
}


int
close_print_obj_fdbk_stats (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_print_obj_fdbk_stats");

    PRINT_TRACE (2, print_string, "Trace: leaving close_print_obj_fdbk_stats");
    return (0);
}
