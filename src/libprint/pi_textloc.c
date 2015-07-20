#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/pi_textloc.c,v 11.2 1993/02/03 16:54:05 smart Exp $";
#endif
/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "textloc.h"
#include "io.h"
#include "buf.h"

#define BUF_LEN 4096
/* Print the interpretation of a TEXTLOC relation to stdout,
    in text form. */
void
print_int_textloc (textloc, output)
TEXTLOC *textloc;
SM_BUF *output;
{
    long size_wanted;
    char buf[BUF_LEN];
    int fd;
    SM_BUF temp_buf;

    if (-1 == (fd = open (textloc->file_name, SRDONLY)) ||
        -1 == lseek (fd, textloc->begin_text, 0)) {
        (void) fprintf (stderr,
                 "Document %ld cannot be read - Ignored \n",
                 textloc->id_num);
	if (fd != -1) (void) close(fd);
        return;
    }

    size_wanted = textloc->end_text - textloc->begin_text;

    if (output == NULL) {
        while (size_wanted > 0) {
            if (-1 == read (fd, &buf[0], (int) MIN(BUF_LEN, size_wanted)) ||
                0 == fwrite (&buf[0], (int) MIN (BUF_LEN, size_wanted),
                                1, stdout)) {
                (void) fprintf (stderr,
                         "Document %ld cannot be read/written - Ignored \n",
                         textloc->id_num);
		(void) close(fd);
                return;
            }
            size_wanted -= BUF_LEN;
        }
        (void) fflush (stdout);
    }
    else {
        /* add text to output instead */
        temp_buf.buf = buf;
        while (size_wanted > 0) {
            if (-1 == read (fd, &buf[0], (int) MIN(BUF_LEN, size_wanted))) {
                (void) fprintf (stderr,
                         "Document %ld cannot be read - Ignored \n",
                         textloc->id_num);
		(void) close(fd);
                return;
            }
            temp_buf.end = MIN(BUF_LEN, size_wanted);
            if (UNDEF == add_buf (&temp_buf, output)) {
                (void) fprintf (stderr,
                         "Document %ld cannot be written - Ignored \n",
                         textloc->id_num);
		(void) close(fd);
                return;
            }
            size_wanted -= BUF_LEN;
        }
    }
    (void) close(fd);
}
