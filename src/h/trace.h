#ifndef TRACEH
#define TRACEH
/*  $Header: /home/smart/release/src/h/trace.h,v 11.2 1993/02/03 16:47:52 smart Exp $ */

#include <stdio.h>
#include "buf.h"
#include "accounting.h"

/* Global trace flag must be true to do any tracing.  Typically set by
   top-level procedure, possibly only for a part of execution.
   See spec parameters global_trace_start, global_trace_end */
extern long global_trace;
extern long global_trace_start, global_trace_end;
extern FILE *global_trace_fd;
extern SM_BUF global_trace_buf;

#ifdef TRACE
/* Per procedure trace flag.  Set by spec file and usage of TRACE_PARAM
   within spec_args */
static long sm_trace;          /* Trace flag. 
                                  0 = none
                                  2 = function enter/leave
                                  4 = output args printed
                                  6 = input args printed
                                  8-? = interior values printed
                                  Odd numbers often used for debugging
                                */


#define PRINT_TRACE(level,proc,arg) \
    if (global_trace && sm_trace >= (level)) { \
        global_trace_buf.end = 0;\
        (void) proc (arg, &global_trace_buf); \
        if (global_accounting) \
            print_accounting ((char *) NULL, &global_trace_buf); \
        (void) fwrite (global_trace_buf.buf, \
                       (int) global_trace_buf.end, 1, global_trace_fd);\
        (void) fflush (global_trace_fd); \
    }

#define TRACE_PARAM(param_name) {param_name, getspec_long, (char *) &sm_trace},

#define SET_TRACE(value) \
  global_trace = ((value) >= global_trace_start && (value) <= global_trace_end)


#else
#define PRINT_TRACE(level,proc,arg)
#define SET_TRACE(value)
#define TRACE_PARAM(param_name)

#endif /* TRACE */
#endif /* TRACEH */
