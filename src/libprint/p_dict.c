#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/p_dict.c,v 11.2 1993/02/03 16:53:48 smart Exp $";
#endif
/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "dict.h"
#include "buf.h"

static SM_BUF internal_output = {0, 0, (char *) 0};

/* Print a DICT relation to stdout */
void
print_dict (dict, output)
DICT_ENTRY *dict;
SM_BUF *output;
{
    SM_BUF *out_p;
    char temp_buf[PATH_LEN];

    if (output == NULL) {
        out_p = &internal_output;
        out_p->end = 0;
    }
    else {
        out_p = output;
    }

    (void) sprintf (temp_buf, 
                    "%ld\t%ld\t%s\n",
                    dict->con,
                    dict->info,
                    dict->token);
    if (UNDEF == add_buf_string (temp_buf, output))
        return;

    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        out_p->end = 0;
    }
}

