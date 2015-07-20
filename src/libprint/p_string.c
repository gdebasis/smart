#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/p_string.c,v 11.2 1993/02/03 16:53:55 smart Exp $";
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
print_string (value, output)
char *value;
SM_BUF *output;
{
    SM_BUF *out_p;

    if (output == NULL) {
        out_p = &internal_output;
        out_p->end = 0;
    }
    else {
        out_p = output;
    }

    if (value != NULL) {
        if (UNDEF == add_buf_string (value, out_p) ||
            UNDEF == add_buf_string ("\n", out_p))
            return;
    }
    else {
        if (UNDEF == add_buf_string ("<NULL>\n", out_p))
            return;
    }

    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        out_p->end = 0;
    }
}

