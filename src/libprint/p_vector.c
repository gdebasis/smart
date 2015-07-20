#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/p_vector.c,v 11.2 1993/02/03 16:53:58 smart Exp $";
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

/* Print a VEC relation to stdout */
void
print_vector (vec, output)
VEC *vec;
SM_BUF *output;
{
    long i;
    CON_WT *conwtp = vec->con_wtp;
    long ctype;
    char temp_buf[PATH_LEN];
    SM_BUF *out_p;

    if (output == NULL) {
        out_p = &internal_output;
        out_p->end = 0;
    }
    else {
        out_p = output;
    }

    for (ctype = 0; ctype < vec->num_ctype; ctype++) {
        for (i = 0; i < vec->ctype_len[ctype]; i++) {
            (void) snprintf (temp_buf,
						    PATH_LEN, 	
                            "%ld\t%ld\t%ld\t%f\n",
                            vec->id_num.id,
                            ctype,
                            conwtp->con,
                            conwtp->wt);
            if (UNDEF == add_buf_string (temp_buf, out_p))
                return;
            if (conwtp >= &vec->con_wtp[vec->num_conwt]) {
                (void) fprintf (stderr,
                      "within print_vector: %ld: Inconsistant vector length\n",
                                vec->id_num.id);
            }
			conwtp++;
        }
    }
    if (conwtp != &vec->con_wtp[vec->num_conwt]) {
        (void) fprintf (stderr,
                "within print_vector: %ld: Inconsistant final vector length\n",
                        vec->id_num.id);
    }
    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        out_p->end = 0;
    }
}

