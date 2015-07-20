#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/p_tokensect.c,v 11.2 1993/02/03 16:54:19 smart Exp $";
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

/* Print a SM_TOKENSECT relation to stdout */
static SM_BUF internal_output = {0, 0, (char *) 0};

void
print_tokensect (tokensect, output)
SM_TOKENSECT *tokensect;
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

    for (i = 0; i < tokensect->num_tokens; i++) {
        if (tokensect->tokens[i].token != NULL) {
            (void) sprintf (temp_buf,
                            "%ld\t'%s'\t%ld\n",
                            tokensect->section_num,
                            tokensect->tokens[i].token,
                            tokensect->tokens[i].tokentype);
        }
        else {
            (void) sprintf (temp_buf,
                            "%ld\t<NULL>\t%ld\n",
                            tokensect->section_num,
                            tokensect->tokens[i].tokentype);
        }
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }

    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        out_p->end = 0;
    }
}
