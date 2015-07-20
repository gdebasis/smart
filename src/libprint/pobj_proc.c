#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/pobj_proc.c,v 11.2 1993/02/03 16:54:19 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 print entire procedure hierarchy
 *1 print.obj.proc
 *2 print_obj_proc (in_file, out_file, inst)
 *3   char *in_file;
 *3   char *out_file;
 *3   int inst;
 *4 init_print_obj_proc (spec, unused)
 *5   "print.trace"
 *4 close_print_obj_proc (inst)
 *7 Print the entire procedure hierarchy.
 *7 Output to go into file "out_file" (if not VALID_FILE, then stdout).
 *9 Should accept in_file as a starting point
***********************************************************************/

#include <fcntl.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "vector.h"
#include "buf.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("print.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);


extern TAB_PROC_TAB proc_root[];
extern int num_proc_root;

static void rec_print_tpt(), rec_print_proc(), print_proc_name();

int
init_print_obj_proc (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_print_obj_proc");

    PRINT_TRACE (2, print_string, "Trace: leaving init_print_obj_proc");
    return (0);
}

int
print_obj_proc (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
    int level = 0;
    FILE *output;
    SM_BUF output_buf;
    
    output = VALID_FILE (out_file) ? fopen (out_file, "w") : stdout;
    if (NULL == output)
        return (UNDEF);
    output_buf.size = 0;

    rec_print_tpt (in_file, proc_root, num_proc_root, &output_buf, level);

    (void) fwrite (output_buf.buf, 1, output_buf.end, output);

    if (output != stdin)
        (void) fclose (output);

    PRINT_TRACE (2, print_string, "Trace: leaving print_obj_proc");
    return (0);
}


int
close_print_obj_proc (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_print_obj_proc");

    PRINT_TRACE (2, print_string, "Trace: leaving close_print_obj_proc");
    return (0);
}

static void
rec_print_tpt (in_file, tpt_ptr, num_entries, output_buf, level)
char *in_file;
TAB_PROC_TAB *tpt_ptr;
int num_entries;
SM_BUF *output_buf;
int level;
{
    long i;
    char *ptr;

    ptr = in_file;
    if (VALID_FILE (in_file)) {
        /* Find first dotted part of in_file. will only work on that */
        while (*ptr && *ptr != '.')
            ptr++;
    }
    
    for (i = 0; i < num_entries; i++) {
        if ((! VALID_FILE (in_file)) ||
            (VALID_FILE (in_file) &&
             0 == strncmp (in_file, tpt_ptr[i].name, (ptr - in_file)))) {
            print_proc_name (tpt_ptr[i].name, output_buf, level);
            if (tpt_ptr[i].type & TPT_TAB) {
                if (ptr && *ptr == '.')
                    ptr++;
                rec_print_tpt (ptr,
                               tpt_ptr[i].tab_proc_tab,
                               *tpt_ptr[i].num_entries,
                               output_buf,
                               level+1);
            }
            if (tpt_ptr[i].type & TPT_PROC) {
                rec_print_proc (tpt_ptr[i].proc_tab,
                                *tpt_ptr[i].num_entries,
                                output_buf,
                                level+1);
            }
        }
    }
}


static void
rec_print_proc (tp_ptr, num_entries, output_buf, level)
PROC_TAB *tp_ptr;
int num_entries;
SM_BUF *output_buf;
int level;
{
    long i;

    for (i = 0; i < num_entries; i++) {
        print_proc_name (tp_ptr[i].name, output_buf, level);
    }
}



static void
print_proc_name (name, output_buf, level)
char *name;
SM_BUF *output_buf;
int level;
{
    char temp_buf[PATH_LEN];
    char *ptr = temp_buf;
    long i;

    for (i = 0; i < level; i++) 
        *ptr++ = '\t';

    (void) strcpy (ptr, name);

    while (*ptr)
        ptr++;
    *ptr++ = '\n';
    *ptr = '\0';

    (void) add_buf_string (temp_buf, output_buf);
}
