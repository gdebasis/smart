#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libgeneral/prepend_db.c,v 11.2 1993/02/03 16:50:43 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include <stdio.h>
#include "param.h"
#include "functions.h"
#include "smart_error.h"

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Return a full path name given a database prefix, and a filename
 *2 prepend_db (database, filename)  returns (char *)
 *3   char *database;
 *3   char *filename;
 *7 Construct a full path name given a database prefix, and a filename
 *7 possibly in that database. If the filename begins with a '/' or '.'
 *7 or '-' it is assumed to be an independent valid path itself and is 
 *7 returned.  If the filename is the empty string, NULL is returned
 *7 Otherwise, the concatenation of the database and the filename is
 *7 returned.

 *9 WARNING: The space allocated here is unreclaimable.  This could 
 *9 be a problem, especially in a server configuration. This should be fixed
 *9 by making this a hierarchy procedure with a close_prepend_db().
***********************************************************************/

/* Construct a full path name given a database prefix, and a filename */
/* possibly in that database. If the filename begins with a '/' or '.' */
/* or '-' it is assumed to be an independent valid path itself and is  */
/* returned.  If the filename is the empty string, NULL is returned */
/* Otherwise, the concatenation of the database and the filename is */
/* returned. */

/* WARNING: The space allocated here is unreclaimable.  This could */
/* be a problem, especially in a server configuration */


static char *full_ptr = NULL;
static char *end_full;

char *
prepend_db (database, filename)
char *database;
char *filename;
{
    register char *ptr;
    char *start_name = NULL;

    if (NULL == full_ptr) {
        if (NULL == (full_ptr = malloc ((unsigned) 8 * PATH_LEN))) {
            set_error (errno, "No space available", "prepend_db");
            return ((char *) NULL);
        }
        end_full = full_ptr + (8 * PATH_LEN);
    }
    switch (filename[0]) {
        case '\0':
            /* filename is invalid, return NULL */
            return ((char *) 0);
        case '-':
        case '.':
        case '/':
            /* filename is valid full pathname, return it */
            return (filename);
        default:
            /* copy database and then filename to full_path, separated by / */
            start_name = full_ptr;
            for (ptr = database;
                 *ptr && full_ptr < end_full; 
                 ptr++, full_ptr++) {
                *full_ptr = *ptr;
            }
            if (full_ptr >= end_full) {
                full_ptr = NULL;
                return (prepend_db (database, filename));
            }
            *full_ptr++ = '/';
            for (ptr = filename; 
                 *ptr && full_ptr < end_full; 
                 ptr++, full_ptr++) {
                *full_ptr = *ptr;
            }
            if (full_ptr >= end_full) {
                full_ptr = NULL;
                return (prepend_db (database, filename));
            }
            *full_ptr++ = '\0';
    }

    return (start_name);
}
