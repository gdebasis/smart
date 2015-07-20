#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/convert.c,v 11.2 1993/02/03 16:49:19 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Top level procedure to convert object in to object out via procedure proc
 *1 convert.convert.convert

 *2 convert (unused1, unused2, inst)
 *3   char *unused1;
 *3   char *unused2;
 *3   int inst;

 *4 init_convert (spec, unused)
 *5   "convert.proc"
 *5   "convert.in"
 *5   "convert.out"
 *5   "convert.trace"
 *4 close_convert(inst)

 *7 A top-level entry point to enable the conversion of one object into
 *7 another.  The procedure "proc" will be called by proc (in, out, inst),
 *7 and will know how to interpret the names "in" and "out".
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "proc.h"

/* Skeleton for a top-level convert program that calls an arbitrary function */
static PROC_INST local_proc;

static char *in_file;
static char *out_file;

static SPEC_PARAM spec_args[] = {
    {"convert.proc",              getspec_func, (char *) &local_proc.ptab},
    {"convert.in",                getspec_dbfile, (char *) &in_file},
    {"convert.out",               getspec_dbfile, (char *) &out_file},
    TRACE_PARAM ("convert.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


int
init_convert (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_convert");

    if (UNDEF == (local_proc.inst = local_proc.ptab->init_proc (spec, NULL))) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving init_convert");

    return (0);
}
int
convert (unused1, unused2, inst)
char *unused1;
char *unused2;
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering convert");

    if (UNDEF == local_proc.ptab->proc (in_file, out_file, local_proc.inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving convert");
    return (0);
}

int
close_convert(inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_convert");

    if (UNDEF == local_proc.ptab->close_proc (local_proc.inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering close_convert");
    return (0);
}
