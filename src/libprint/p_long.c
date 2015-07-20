#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/p_long.c,v 11.2 1993/02/03 16:53:51 smart Exp $";
#endif
/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include <stdio.h>
#include <functions.h>
#include "common.h"
#include "buf.h"

static SM_BUF internal_output = {0, 0, (char *) 0};
void
print_long (value, output)
long *value;
SM_BUF *output;
{
    SM_BUF *out_p;
    char temp_buf[64];

    if (output == NULL) {
        out_p = &internal_output;
        out_p->end = 0;
    }
    else {
        out_p = output;
    }

    (void) sprintf (temp_buf, "%ld\n", *value);
    if (UNDEF == add_buf_string (temp_buf, out_p))
        return;

    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        out_p->end = 0;
    }
}

