#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/print.c,v 11.2 1993/02/03 16:54:01 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Top level procedure to print object in to file out via procedure proc
 *1 print.print.print

 *2 print (unused1, unused2, inst)
 *3   char *unused1;
 *3   char *unused2;
 *3   int inst;

 *4 init_print (spec, unused)
 *5   "print.proc"
 *5   "print.in"
 *5   "print.out"
 *5   "print.trace"
 *4 close_print(inst)

 *7 A top-level entry point to print one object into the file out.
 *7 The procedure "proc" will be called by proc (in, out, inst),
 *7 and will know how to interpret the names "in" and "out".
 *7 If "out" is not a valid file name (eg "-") then print to stdout.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "proc.h"

/* Skeleton for a top-level print program that calls an arbitrary function */
static PROC_INST local_proc;

static char *in_file;
static char *out_file;

static SPEC_PARAM spec_args[] = {
    {"print.proc",              getspec_func, (char *) &local_proc.ptab},
    {"print.in",                getspec_string, (char *) &in_file},
    {"print.out",               getspec_string, (char *) &out_file},
    TRACE_PARAM ("print.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


int
init_print (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_print");

    if (UNDEF == (local_proc.inst = local_proc.ptab->init_proc (spec, NULL))) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving init_print");

    return (0);
}
int
print (unused1, unused2, inst)
char *unused1;
char *unused2;
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering print");

    if (UNDEF == local_proc.ptab->proc (in_file, out_file, local_proc.inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving print");
    return (0);
}

int
close_print(inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_print");

    if (UNDEF == local_proc.ptab->close_proc (local_proc.inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering close_print");
    return (0);
}
