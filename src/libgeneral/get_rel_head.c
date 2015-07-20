#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libgeneral/get_rel_head.c,v 11.2 1993/02/03 16:50:37 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Return the header at the beginning of SMART relational object rel_object.
 *2 get_rel_header (rel_object)
 *3   char *rel_object;
 *7 Open the file rel_object and read the first sizeof (REL_HEADER) bytes,
 *7 returning them as an object of type REL_HEADER.
 *7 NULL is returned if error.
 *9 REL_HEADER returned is a pointer to static storage, and is over-written
 *9 upon next call.
***********************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "rel_header.h"
#include "io.h"
#include "smart_error.h"

static REL_HEADER rh;

REL_HEADER *
get_rel_header (rel_object)
register char *rel_object;
{
    register int fd;

    if (-1 == (fd = open (rel_object, SRDONLY))) {
        set_error (errno, rel_object, "_direct");
        return ((REL_HEADER *) NULL);
    }
    if (sizeof (REL_HEADER) != read (fd, (char *) &rh, sizeof (REL_HEADER))) {
        (void) close (fd);
        set_error (errno, rel_object, "_direct");
        return ((REL_HEADER *) NULL);
    }

    (void) close (fd);
    return (&rh);
}
