#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/p_sm_condoc.c,v 11.2 1993/02/03 16:54:16 smart Exp $";
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

/* Print a SM_INDEX_CONDOC relation to stdout or memory */
void
print_sm_condoc (condoc, output)
SM_CONDOC *condoc;
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

     for (i = 0; i < condoc->num_sections; i++) {
        for (j = 0; j < condoc->sections[i].num_concepts; j++) {
            (void) sprintf (temp_buf,
                            "%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\n",
                            condoc->id_num,
                            i,
                            condoc->sections[i].concepts[j].con,
                            condoc->sections[i].concepts[j].ctype,
                            condoc->sections[i].concepts[j].para_num,
                            condoc->sections[i].concepts[j].sent_num,
                            condoc->sections[i].concepts[j].token_num);
            if (UNDEF == add_buf_string (temp_buf, out_p))
                return;
        }
    }

    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        out_p->end = 0;
    }
}
