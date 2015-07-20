#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/p_tr_vec.c,v 11.2 1993/02/03 16:53:57 smart Exp $";
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
#include "tr_vec.h"

static SM_BUF internal_output = {0, 0, (char *) 0};

void
print_tr_vec (tr_vec, output)
TR_VEC *tr_vec;
SM_BUF *output;
{
    SM_BUF *out_p;
    TR_TUP *tr_tup;
    char temp_buf[PATH_LEN];

    if (output == NULL) {
        out_p = &internal_output;
        out_p->end = 0;
    }
    else {
        out_p = output;
    }

    for (tr_tup = tr_vec->tr;
         tr_tup < &tr_vec->tr[tr_vec->num_tr];
         tr_tup++) {
        (void) sprintf (temp_buf,
                        "%ld\t%ld\t%ld\t%d\t%d\t%d\t%9.4f\n",
                        tr_vec->qid,
                        tr_tup->did,
                        tr_tup->rank,
                        (int) tr_tup->rel,
                        (int) tr_tup->action,
                        (int) tr_tup->iter,
                        tr_tup->sim);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }
    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        out_p->end = 0;
    }

}

