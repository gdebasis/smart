#ifdef RCSID
static char *rcsid = "$Header: /home/smart/release/src/libgeneral/backup.c,v 11.2 1993/02/03 16:50:35 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Utility program to Move the file <in_file> to name "<in_file>.bk".
 *2 backup (in_file)
 *3   char *in_file;
 *7 Normally called when a file is to be written and new copy is incore,
 *7 so no need to waste time copying file.
***********************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "io.h"
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"

int
backup (in_file)
char *in_file;
{
    char buf[PATH_LEN];
    struct stat stat_buf;
    int fd;

    (void) sprintf (buf, "%s.bk", in_file);
    if (-1 == link (in_file, buf)) {
        set_error (errno, in_file, "backup");
        return (UNDEF);
    }
    if (-1 == stat (in_file, &stat_buf)) {
        set_error (errno, in_file, "backup");
        return (UNDEF);
    }
    
    if (-1 == unlink (in_file)) {
        set_error (errno, in_file, "backup");
        return (UNDEF);
    }

#ifdef BSD4_2
    if (-1 == (fd = open (in_file, SCREATE | SWRONLY, stat_buf.st_mode))) {
        set_error (errno, in_file, "backup");
        return (UNDEF);
    }
#else
    if (-1 == (fd = creat (in_file, (int)stat_buf.st_mode))) {
        set_error (errno, in_file, "backup");
        return (UNDEF);
    }
#endif

    (void) close (fd);
    return (0);
}

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Utility program to copy the file <in_file> to name "<in_file>.bk".
 *2 prepare_backup (file_name)
 *3   char *file_name;
 *2 make_backup (file_name)

 *7 Prepare_backup (file_name) and make_backup (file_name) are a pair of 
 *7 procedures used for the same purpose as "backup" : eventually putting 
 *7 the old copy of the file in <file_name>.bk and the new copy in 
 *7 <file_name>.  This pair does not have the large concurrency problem 
 *7 window that backup has. (If some other process tries to access  
 *7 <file_name> just after the call to backup but before the calling 
 *7 procedure finishes writing out <file_name>, this other process bombs.) 
 *7     
 *7 Prepare_backup returns a file descriptor suitable for writing. This
 *7 file descriptor is actually to <file_name>.nw although the calling 
 *7 procedure does not realize this.  The calling procedure then writes 
 *7 out the new version of <file_name>, closes the file descriptor, and 
 *7 calls make_backup (filename).  
 *7 Make_backup moves the present <file_name> to <file_name>.bk, and then 
 *7 moves <file_name>.nw to <file_name>. 
 *7 Note there is still a slight window of vulnerability here; the two moves 
 *7 are not done as an atomic operation. It is possible for another process 
 *7 to sneak in between them. 

***********************************************************************/

int
prepare_backup (file_name)
char *file_name;
{
    int fd;
    struct stat stat_buf;
    char new_path[PATH_LEN];
    
    (void) sprintf (new_path, "%s.nw", file_name);
    
    if (-1 == stat (file_name, &stat_buf)) {
        set_error (errno, file_name, "prepare_backup");
        return (UNDEF);
    }
    
#ifdef BSD4_2
    if (-1 == (fd = open (new_path, 
                          SCREATE | SRDWR, 
                          (int) stat_buf.st_mode))) {
        set_error (errno, new_path, "backup");
        return (UNDEF);
    }
#else
    /* BUG?? fd may need to accessed for reading */
    if (-1 == (fd = creat (new_path, (int) stat_buf.st_mode))) {
        set_error (errno, new_path, "backup");
        return (UNDEF);
    }
#endif

    return (fd);
}

int
make_backup (file_name)
char *file_name;
{
    char new_path[PATH_LEN];
    char bak_path[PATH_LEN];
    
    (void) sprintf (new_path, "%s.nw", file_name);
    (void) sprintf (bak_path, "%s.bk", file_name);
    
    if (-1 == link (file_name, bak_path) ||
        -1 == unlink (file_name) ||
        -1 == link (new_path, file_name) ||
        -1 == unlink (new_path)){
        set_error (errno, file_name, "make_backup");
        return (UNDEF);
    }
    
    return (0);
}
