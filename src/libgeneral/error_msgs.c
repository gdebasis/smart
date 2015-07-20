#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libgeneral/error_msgs.c,v 11.2 1993/02/03 16:50:39 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 print a SMART error message
 *2 print_error (new_routine, new_message)
 *3  char *new_routine;
 *3  char *new_message;
 *6 Global UNIX variables errno, sys_nerr, sys_errlist are used, as well
 *6 as SMART global variables smart_errlist and smart_errno;

 *7 Print an error message to stderr.  At point of error determination,
 *7 either smart_errno should be set, or (if UNIX library error) errno will
 *7 be set.  If smart_errno is set, then the routine name that detected the
 *7 error and a message are printed.  In addition, the routine name that prints
 *7 the error and it's message (eg action to be taken) are printed.
 *9 smart_errno should be more widely used, in particular to locate the
 *9 procedure the error occurs in.  Many errors can only get "located"
 *9 by setting trace.
***********************************************************************/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 print a SMART error message to a SM_BUF
 *2 print_error_buf (new_routine, new_message, sm_buf)
 *3  char *new_routine;
 *3  char *new_message;
 *3 SM_BUF *sm_buf;
 *6 Global UNIX variables errno, sys_nerr, sys_errlist are used, as well
 *6 as SMART global variables smart_errlist and smart_errno;

 *7 As above, except that the error message is added to sm_buf.
***********************************************************************/

#include <stdio.h>
#include "functions.h"
#include "smart_error.h"
#include "buf.h"
#include "common.h"

extern int  errno;
extern int  sys_nerr;
/*extern char *sys_errlist[];*/

static char *smart_errlist[] = {
    "Inconsistency check",
    "Illegal value for seek",
    "Illegal mode for object",
    "Illegal parameter value"
};

void
print_error (new_routine, new_message)
char *new_routine;
char *new_message;
{
    if (smart_errno > 0 && smart_errno < sys_nerr) {
        (void) fprintf (stderr, "%s: in %s: '%s' %s - %s\n",
                 new_routine,
                 smart_routine,
                 smart_message,
                 strerror(smart_errno),
                 new_message);
    }
    else if (smart_errno >= SMART_MINERR && 
             smart_errno < SMART_MINERR + SMART_NUMERR) {
        (void) fprintf (stderr, "%s: in %s: '%s' %s - %s\n",
                 new_routine,
                 smart_routine,
                 smart_message,
                 smart_errlist[smart_errno - SMART_MINERR],
                 new_message);
    }
    else if (smart_errno == 0 && errno != 0) {
        /* Presumably error detected directly by new_routine */
        /* after system call */
        (void) fprintf (stderr, "%s: '%s' - %s\n",
                 new_routine,
                 strerror(errno),
                 new_message);
    }
    else {
        (void) fprintf (stderr, "%s: Undetermined error detected - %s\n",
                 new_routine,
                 new_message);
    }

    /* Reset the global error indicators */
    errno = 0;
    smart_errno = 0;
    smart_message = NULL;
    smart_routine = NULL;
}

void
print_error_buf (new_routine, new_message, sm_buf)
char *new_routine;
char *new_message;
SM_BUF *sm_buf;
{
    char mesg_buf[PATH_LEN];

    if (smart_errno > 0 && smart_errno < sys_nerr) {
        (void) sprintf (mesg_buf, "%s: in %s: '%s' %s - %s\n",
                 new_routine,
                 smart_routine,
                 smart_message,
                 sys_errlist[smart_errno],
                 new_message);
    }
    else if (smart_errno >= SMART_MINERR && 
             smart_errno < SMART_MINERR + SMART_NUMERR) {
        (void) sprintf (mesg_buf, "%s: in %s: '%s' %s - %s\n",
                 new_routine,
                 smart_routine,
                 smart_message,
                 smart_errlist[smart_errno - SMART_MINERR],
                 new_message);
    }
    else if (smart_errno == 0 && errno != 0) {
        /* Presumably error detected directly by new_routine */
        /* after system call */
        (void) sprintf (mesg_buf, "%s: '%s' - %s\n",
                 new_routine,
                 sys_errlist[errno],
                 new_message);
    }
    else {
        (void) sprintf (mesg_buf, "%s: Undetermined error detected - %s\n",
                 new_routine,
                 new_message);
    }

    (void) add_buf_string(mesg_buf, sm_buf);

    /* Reset the global error indicators */
    errno = 0;
    smart_errno = 0;
    smart_message = NULL;
    smart_routine = NULL;
}
