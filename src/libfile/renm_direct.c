#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfile/renm_direct.c,v 11.2 1993/02/03 16:50:31 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "direct.h"
#include "smart_error.h"
#include "io.h"

/* Rename an instance of a 'direct' relational object. */
int
rename_direct (in_file_name, out_file_name)
char *in_file_name;
char *out_file_name;
{
    char in_buf[PATH_LEN+4];
    char out_buf[PATH_LEN+4];
    int fd, i;
    REL_HEADER rh;

    if (-1 == (fd = open(in_file_name, SRDONLY))) {
	rh.num_var_files = 1;
    } else {
	if (sizeof(REL_HEADER) != read(fd, (char *)&rh, sizeof(REL_HEADER)))
	    rh.num_var_files = 1;
	(void) close(fd);
    }

    /* Link out_file_name to the existing in_file_name relation */
    /* Maintain invariant that if the main file_name of a relation
       exists, it is a complete relation */

    /* link the "which" file */
    if (rh.num_var_files > 1) {
	(void) sprintf (in_buf, "%s.which", in_file_name);
	(void) sprintf (out_buf, "%s.which", out_file_name);

	if (-1 == link (in_buf, out_buf)) {
	    set_error (errno, in_buf, "rename_direct");
	    return (UNDEF);
	}
    }

    /* link the var files */
    for (i = 0; i < rh.num_var_files; i++) {
	(void) sprintf (in_buf, "%s.v%03d", in_file_name, i);
	(void) sprintf (out_buf, "%s.v%03d", out_file_name, i);

	if (-1 == link (in_buf, out_buf)) {
	    set_error (errno, in_buf, "rename_direct");
	    return (UNDEF);
	}
    }

    /* link the fix file */
    if (-1 == link (in_file_name, out_file_name)) {
        set_error (errno, in_file_name, "rename_direct");
        return (UNDEF);
    }

    /* Unlink the old filenames, fix file first */
    if (-1 == unlink (in_file_name)) {
        set_error (errno, in_file_name, "rename_direct");
        return (UNDEF);
    }

    (void) sprintf (in_buf, "%s.which", in_file_name);
    if (-1 == unlink (in_buf)) {
        set_error (errno, in_buf, "rename_direct");
        return (UNDEF);
    }

    for (i = 0; i < rh.num_var_files; i++) {
	(void) sprintf (in_buf, "%s.v%03d", in_file_name, i);
	if (-1 == unlink (in_buf)) {
	    set_error (errno, in_buf, "rename_direct");
	    return (UNDEF);
	}
    }

    return (0);
}
