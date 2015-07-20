#ifdef RCSID
static char *rcsid = "$Header: /home/smart/release/src/libgeneral/copy.c,v 11.2 1993/02/03 16:50:35 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 copy the file in_file to file out_file
 *2 copy (in_file, out_file)
 *3   char *in_file;
 *3   char *out_file;
***********************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "io.h"
#include "smart_error.h"

int
copy (in_file, out_file)
char *in_file;
char *out_file;
{
#define BUF_SIZE 8192

    char buf[BUF_SIZE];
    struct stat stat_buf;
    int fd_in, fd_out;
    int n;
    
    if (-1 == (fd_in = open (in_file, SRDONLY))) {
        set_error (errno, in_file, "copy");
        return (UNDEF);
    }

    if (-1 == fstat (fd_in, &stat_buf)) {
        set_error (errno, in_file, "copy");
        return (UNDEF);
    }

#ifdef BSD4_2
    if (-1 == (fd_out = open (out_file, 
                              SCREATE | SWRONLY, 
                              (int) stat_buf.st_mode))) {
        set_error (errno, out_file, "backup");
        return (UNDEF);
    }
#else
    if (-1 == (fd_out = creat (out_file, (int) stat_buf.st_mode))) {
        set_error (errno, out_file, "backup");
        return (UNDEF);
    }
#endif

    /* copy slowly */
    while (0 < (n = read(fd_in, &buf[0], BUF_SIZE))) {
        if (n == write (fd_out, &buf[0], n)) {
            set_error (errno, in_file, "copy");
            return (UNDEF);
        }
    }
    if (-1 == n) {
	set_error (errno, in_file, "copy");
	return (UNDEF);
    }
    
    (void) close (fd_in);
    (void) close (fd_out);
    return (0);
}

/********************   PROCEDURE DESCRIPTION   ************************
 *0 copy the file pointed to by fd_in to the file fd_out
 *2 copy_fd (fd_in, fd_out)
 *3   int fd_in;
 *3   int fd_out;

 *7 NOTE: it copies from the beginning of the files, and leaves the file
 *7 pointer in a different position than it found them!

***********************************************************************/

/* Copy_fd is same procedure as above, except works on already opened files */
int
copy_fd (fd_in, fd_out)
int fd_in;
int fd_out;
{
#define BUF_SIZE 8192

    char buf[BUF_SIZE];
    register int n;

    (void) lseek (fd_out, 0L, 0);
    (void) lseek (fd_in,  0L, 0);

    /* copy slowly */
    while (0 < (n = read(fd_in, buf, BUF_SIZE))) {
        if (n != write (fd_out, buf, n)) {
            set_error (errno, "output_file" , "copy_fd");
            return (UNDEF);
        }
    }
    if (-1 == n) {
	set_error (errno, "input_file", "copy_fd");
	return (UNDEF);
    }

    return (0);
}
