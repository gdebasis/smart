#ifndef ACCOUNTINGH
#define ACCOUNTINGH


/* Global variable giving whether and how much accounting information should
   be printed.  If positive, then any time a PRINT_TRACE succeeds (see
   trace.h, accounting info is printed.  If 1, then just time of day is
   printed (giving elapsed tim between two calls, eg. entering and leaving
   a procedure.)  If 2, additionally prints max_memory used, user and system
   time used.  If 3 or more, prints fault info, io info, signal info and
   context switch info.  (All of this is if ACCOUNTING flag is compiled
   see param.h).
*/

extern long global_accounting;

#endif /* ACCOUNTINGH */
