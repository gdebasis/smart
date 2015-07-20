#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfile/creat_direct.c,v 11.2 1993/02/03 16:50:17 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "rel_header.h"
#include "direct.h"
#include "io.h"
#include "smart_error.h"

extern _SDIRECT _Sdirect[];

extern int create_var_file(), create_which_file();

/* Create an instance of a 'direct' relational object.  The input rel_header
 * has the type and _entry_size set, while the other fields are filled in
 * here.
 *
 * A "which" file is not needed until the object overflows its first var
 * file, so it is not created until then.
 */
int
create_direct (file_name, rh)
char *file_name;
REL_HEADER *rh;
{
    int fd;

    if (-1 != (fd = open (file_name, 0))) {
        (void) close (fd);
        set_error (EEXIST, file_name, "create_direct");
        return (UNDEF);
    }

    /* Open and close the initial var file */
    if (UNDEF == create_var_file(file_name, 0))
	return(UNDEF);

    if (-1 == (fd = open (file_name, SWRONLY | SCREATE, 0664))) {
        set_error (errno, file_name, "create_direct");
        return (UNDEF);
    }

#ifdef notdef
    /* Alternative in case some ancient version of UNIX can't give
       mode on open line */
    if ( -1 == (fd = creat (file_name, 0664))) {
        set_error (errno, file_name, "create_direct");
        return (UNDEF);
    }
#endif

    rh->max_primary_value = -1;
    rh->max_secondary_value = -1;
    rh->num_entries = 0;
    rh->num_var_files = 1;
    rh->_internal = 0;

    /* Write the relation header */
    if (sizeof (REL_HEADER) != write (fd, (char *) rh, sizeof (REL_HEADER))){
        set_error (errno, file_name, "create_direct");
        return (UNDEF);
    }

    (void) close (fd);
    return (0);
}

int
create_var_file(file_name, i)
char *file_name;
int i;
{
    int fd;
    char buf[PATH_LEN];

    (void) sprintf (buf, "%s.v%03d", file_name, i);

    if (-1 == (fd = open (buf, SWRONLY | SCREATE, 0664))) {
        set_error (errno, buf, "create_direct");
        return (UNDEF);
    }

#ifdef notdef
    /* Alternative in case some ancient version of UNIX can't give
       mode on open line */
    if ( -1 == (fd = creat (buf, 0664))) {
        set_error (errno, buf, "create_direct");
        return (UNDEF);
    }
#endif

    (void) close (fd);

    return(0);
}

/* create "which" file, and initialize it with 0's for each entry in the
 * relation
 */
int
create_which_file(dir_index)
int dir_index;
{
    register _SDIRECT *dir_ptr = &(_Sdirect[dir_index]);
    int fd;
    char buf[PATH_LEN];
    long to_write;
    int this_time;

    (void) sprintf(buf, "%s.which", dir_ptr->file_name);

    if (-1 == (fd = open(buf, SWRONLY | SCREATE, 0664))) {
	set_error(errno, buf, "create_which_file");
	return(UNDEF);
    }

#ifdef notdef
    /* Alternative in case some ancient version of UNIX can't give
       mode on open line */
    if ( -1 == (fd = creat (buf, 0664))) {
        set_error (errno, buf, "create_which_file");
        return (UNDEF);
    }
#endif

    (void) memset(buf, '\0', PATH_LEN);
    to_write = dir_ptr->rh.max_primary_value + 1;

    while (to_write > 0) {
	if (to_write > PATH_LEN)
	    this_time = PATH_LEN;
	else
	    this_time = to_write;

	if (this_time != write(fd, buf, this_time)) {
	    set_error(errno, "which file", "create_which_file");
	    return(UNDEF);
	}
	to_write -= this_time;
    }

    (void) close(fd);

    return(0);
}
