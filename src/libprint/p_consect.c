#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/p_consect.c,v 11.2 1993/02/03 16:53:47 smart Exp $";
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
#include "docindex.h"
#include "buf.h"

static SM_BUF internal_output = {0, 0, (char *) 0};

/* Print a SM_CONSECT relation to stdout or memory */
void
print_consect (consect, output)
SM_CONSECT *consect;
SM_BUF *output;
{
    long i;
    SM_BUF *out_p;
    char temp_buf[PATH_LEN];

    if (output == NULL) {
        out_p = &internal_output;
        out_p->end = 0;
    }
    else {
        out_p = output;
    }

    for (i = 0; i < consect->num_concepts; i++) {
        (void) sprintf (temp_buf,
                        "%ld\t%ld\t%ld\t%ld\t%ld\n",
                        consect->concepts[i].con,
                        consect->concepts[i].ctype,
                        consect->concepts[i].para_num,
                        consect->concepts[i].sent_num,
                        consect->concepts[i].token_num);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }

    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        out_p->end = 0;
    }
}
