#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libgeneral/text_array.c,v 11.2 1993/02/03 16:50:46 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Utility procedure to read all lines of a file into an array
 *2 text_long_array (filename, long_array, num_long_array)
 *3  char *filename;
 *3  long **long_array;
 *3  long *num_long_array;
 *7 Each line of filename is assumed to be a single long integer.  All lines
 *7 are read and placed into long_array.  Both long_array and num_long_array
 *7 are set.
 *7 Return UNDEF if filename if error, 1 otherwise.
 *7 Use free_long_array (filename, long_array, num_long_array) to
 *7 free the space malloced by text_long_array.
***********************************************************************/

#include <ctype.h>
#include "common.h"
#include "functions.h"

int 
text_long_array (filename, long_array, num_long_array)
char *filename;
long **long_array;
long *num_long_array;
{
    char buf[PATH_LEN];
    long *array;
    long num_array;
    long file_size;
    FILE *fd;

    if (! (VALID_FILE (filename)))
        return (UNDEF);
    if (NULL == (fd = fopen (filename, "r")))
        return (UNDEF);
    if (-1 == fseek (fd, 0L, 2) ||
	-1 == (file_size = ftell (fd)) ||
        -1 == fseek (fd, 0L, 0)) {
        (void) fclose (fd);
        return (UNDEF);
    }
    if (NULL == (array = (long *)
                 malloc ((unsigned) file_size * (sizeof (long) / 2))))
        return (UNDEF);

    num_array = 0;
    while (NULL != fgets (buf, PATH_LEN, fd)) {
        array[num_array++] = atol (buf);
    }
    assert( num_array <= file_size * (sizeof(long) / 2) );

    (void) fclose (fd);
    *long_array = array;
    *num_long_array = num_array;

    return (1);
}


int 
free_long_array (filename, long_array, num_long_array)
char *filename;
long **long_array;
long *num_long_array;
{
    if (*num_long_array > 0) 
        (void) free ((char *) long_array);
    return (0);
}
