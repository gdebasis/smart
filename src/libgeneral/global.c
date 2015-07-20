#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libgeneral/global.c,v 11.2 1993/02/03 16:50:36 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Global variables used within SMART
 *2 smart_errno
 *2 smart_message
 *2 smart_routine
 *2 global_context
 *2 global_trace
 *2 global_trace_start
 *2 global_trace_end
 *2 global_accounting
 *2 global_start
 *2 global_end
 *7 
 *7 * Declarations of external variables defined in "smart_error.h" *
 *7 
 *7 long smart_errno;               * If > 0 and <= sys_nerr then refers to  *
 *7                                 * sys_errlist, else if >= smart_errmin *
 *7                                 * and <= smart_errmax, then smart_errlist *
 *7 char *smart_message;            * Message to be printed (often filename) *
 *7 char *smart_routine;            * Major routine issuing error message *
 *7 
 *7 * Global variable describing context in which procedures are executing.
 *7    For example, if weighting a vector, need to know whether to use
 *7    doc_weight or query_weight values. (eg. global_context & INDEXING_DOC)
 *7    See context.h for full description.  *
 *7 long global_context = 0;
 *7 
 *7 * global_variable describing whether tracing is potentially on.  See
 *7    trace.h.   By default, tracing is enabled globally but disabled at
 *7    every local location *
 *7 long global_trace = 1;
 *7 long global_trace_start;
 *7 long global_trace_end;
 *7 FILE *global_trace_fd;  * stdout, unless parameter global_trace_file
 *7                           has been given a value *
 *7 SM_BUF *global_trace_buf;  * Internal buffer used by trace routines *
 *7
 *7 * global variable indicating if accounting is turned on.  See trace.h
 *7    and accounting.h.
 *7    If on, then every trace message will also print accounting info.
 *7    By default, off *
 *7 long global_accounting = 0;
 *7 
 *7 * Global variables describing potential starting or stopping points of
 *7    execution. *
 *7 long global_start, global_end;
 *7 
***********************************************************************/

#include <stdio.h>
#include "buf.h"

/* Declarations of external variables defined in "smart_error.h" */

long smart_errno;               /* If > 0 and <= sys_nerr then refers to  */
                                /* sys_errlist, else if >= smart_errmin */
                                /* and <= smart_errmax, then smart_errlist */
char *smart_message;            /* Message to be printed (often filename) */
char *smart_routine;            /* Major routine issuing error message */

/* Global variable describing context in which procedures are executing.
   For example, if weighting a vector, need to know whether to use
   doc_weight or query_weight values. (eg. global_context & INDEXING_DOC)
   See context.h for full description.  */
long global_context = 0;

/* global_variable describing whether tracing is potentially on.  See
   trace.h.   By default, tracing is enabled globally but disabled at
   every local location */
long global_trace = 1;
long global_trace_start;
long global_trace_end;
FILE *global_trace_fd;
SM_BUF global_trace_buf = {0, 0, (char *) NULL};

/* global variable indicating if accounting is turned on.  See trace.h
   and accounting.h.
   If on, then every trace message will also print accounting info.
   By default, off */
long global_accounting = 0;

/* Global variables describing potential starting or stopping points of
   execution. */
long global_start, global_end;




