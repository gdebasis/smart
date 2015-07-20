#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/p_tokendoc.c,v 11.2 1993/02/03 16:54:18 smart Exp $";
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

/* Print a SM_TOKENDOC relation to stdout */
static SM_BUF internal_output = {0, 0, (char *) 0};

void
print_tokendoc (tokendoc, output)
SM_TOKENDOC *tokendoc;
SM_BUF *output;
{
    long i,j;
    SM_BUF *out_p;
    char temp_buf[PATH_LEN];

    if (output == NULL) {
        out_p = &internal_output;
        out_p->end = 0;
    }
    else {
        out_p = output;
    }

    for (i = 0; i < tokendoc->num_sections; i++) {
        for (j = 0; j < tokendoc->sections[i].num_tokens; j++) {
            if (tokendoc->sections[i].tokens[j].token != NULL) {
                (void) sprintf (temp_buf,
                                "%ld\t%ld\t%ld\t'%s'\t%ld\n",
                                tokendoc->id_num,
                                i,
                                tokendoc->sections[i].section_num,
                                tokendoc->sections[i].tokens[j].token,
                                tokendoc->sections[i].tokens[j].tokentype);
            }
            else {
                (void) sprintf (temp_buf,
                                "%ld\t%ld\t%ld\t<NULL>\t%ld\n",
                                tokendoc->id_num,
                                i,
                                tokendoc->sections[i].section_num,
                                tokendoc->sections[i].tokens[j].tokentype);
            }
            if (UNDEF == add_buf_string (temp_buf, out_p))
                return;
        }
    }

    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        out_p->end = 0;
    }
}
