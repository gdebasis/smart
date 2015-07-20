#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/p_fdbk_info.c,v 11.2 1993/02/03 16:53:48 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "vector.h"
#include "retrieve.h"
#include "feedback.h"
#include "buf.h"

static SM_BUF internal_output = {0, 0, (char *) 0};

void
print_fdbk_info (info, output)
FEEDBACK_INFO *info;
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

    (void) sprintf (temp_buf, "Query %5ld, num_rel %5ld, num_ret %5ld\n",
                   info->orig_query->id_num.id, info->num_rel, info->tr->num_tr);
    if (UNDEF == add_buf_string (temp_buf, out_p))
        return;
    (void) sprintf (temp_buf,
                    "  Con ctype Q? rr nr nret wt orig_wt relwt nrelwt\n");
    if (UNDEF == add_buf_string (temp_buf, out_p))
        return;
    for (i = 0; i < info->num_occ; i++) {
        (void) sprintf (temp_buf,
                    "  %5ld %2ld %1d %2ld %2ld %4ld %6.4f %6.4f %6.4f %6.4f\n",
                        info->occ[i].con, info->occ[i].ctype,
                        info->occ[i].query, info->occ[i].rel_ret,
                        info->occ[i].nrel_ret, info->occ[i].nret,
                        info->occ[i].weight, info->occ[i].orig_weight,
                        info->occ[i].rel_weight, info->occ[i].nrel_weight);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }

    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        out_p->end = 0;
    }
}
