#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/p_buf.c,v 11.2 1993/02/03 16:53:58 smart Exp $";
#endif
/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "buf.h"


/* Print a SM_BUF relation to stdout */
void
print_buf (in_buf, output)
SM_BUF *in_buf;
SM_BUF *output;
{
    if (output == NULL)
        (void) fwrite (in_buf->buf, 1, in_buf->end, stdout);
    else
        (void) add_buf (in_buf, output);
}

