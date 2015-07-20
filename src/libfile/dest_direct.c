#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfile/dest_direct.c,v 11.2 1993/02/03 16:50:20 smart Exp $";
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

/* Destroy an instance of a 'direct' relational object. */
int
destroy_direct (file_name)
char *file_name;
{
    char buf[PATH_LEN];
    int fd, i;
    REL_HEADER rh;

    if (-1 == (fd = open(file_name, SRDONLY))) {
	rh.num_var_files = 1;
    } else {
	if (sizeof(REL_HEADER) != read(fd, (char *)&rh, sizeof(REL_HEADER)))
	    rh.num_var_files = 1;
	(void) close(fd);
    }

    /* unlink the fix file */
    if (-1 == unlink (file_name)) {
        set_error (errno, file_name, "destroy_direct");
        return (UNDEF);
    }

    /* unlink the "which" file */
    if (rh.num_var_files > 1) {
	(void) sprintf (buf, "%s.which", file_name);
	if (-1 == unlink (buf)) {
	    set_error (errno, buf, "destroy_direct");
	    return (UNDEF);
	}
    }

    /* unlink the var files */
    for (i = 0; i < rh.num_var_files; i++) {
	(void) sprintf (buf, "%s.v%03d", file_name, i);
	if (-1 == unlink (buf)) {
	    set_error (errno, buf, "destroy_direct");
	    return (UNDEF);
	}
    }

    return (0);
}
