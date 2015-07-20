#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/p_vec_list.c,v 11.2 1993/02/03 16:54:20 smart Exp $";
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
#include "vector.h"
#include "buf.h"

static SM_BUF internal_output = {0, 0, (char *) 0};

/* Print a VEC_LIST relation to stdout */
void
print_vec_list (vec_list, output)
VEC_LIST *vec_list;
SM_BUF *output;
{
    long i, j;
    CON_WT *conwtp;
    long ctype;
    SM_BUF *out_p;
    VEC *vec;
    char temp_buf[PATH_LEN];

    if (output == NULL) {
        out_p = &internal_output;
        out_p->end = 0;
    }
    else {
        out_p = output;
    }

    for (i = 0; i < vec_list->num_vec; i++) {
        vec = &vec_list->vec[i];
        conwtp = vec->con_wtp;
        for (ctype = 0; ctype < vec->num_ctype; ctype++) {
            for (j = 0; j < vec->ctype_len[ctype]; j++) {
                (void) sprintf (temp_buf, 
                                "%ld%ld\t%ld\t%ld\t%f\n",
                                vec_list->id_num.id,
                                vec->id_num.id,
                                ctype,
                                conwtp->con,
                                conwtp->wt);
                if (UNDEF == add_buf_string (temp_buf, out_p))
                    return;
                if (++conwtp > &vec->con_wtp[vec->num_conwt]) {
                    (void) fprintf (stderr,
                       "print_veclist: %ld: Inconsistant vector length\n",
                                    vec->id_num.id);
                }
            }
        }
        if (conwtp != &vec->con_wtp[vec->num_conwt]) {
            (void) fprintf (stderr,
                      "print_veclist: %ld: Inconsistant final vector length\n",
                            vec->id_num.id);
        }
    }

    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        out_p->end = 0;
    }
}

