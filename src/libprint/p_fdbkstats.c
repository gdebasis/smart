#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/p_fdbk_stats.c,v 11.0 1992/07/21 18:23:16 chrisb Exp $";
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
#include "fdbk_stats.h"

static SM_BUF internal_output = {0, 0, (char *) 0};

void
print_fdbk_stats (fdbk_stats, output)
FDBK_STATS *fdbk_stats;
SM_BUF *output;
{
    SM_BUF *out_p;
    OCCINFO *occinfo;
    char temp_buf[PATH_LEN];

    if (output == NULL) {
        out_p = &internal_output;
        out_p->end = 0;
    }
    else {
        out_p = output;
    }

    for (occinfo = fdbk_stats->occinfo;
         occinfo < &fdbk_stats->occinfo[fdbk_stats->num_occinfo];
         occinfo++) {
        (void) sprintf (temp_buf,
                    "%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%9.4f\t%9.4f\t%ld\t%ld\n",
                        fdbk_stats->id_num,
                        occinfo->ctype,
                        occinfo->con,
                        occinfo->rel_ret,
                        occinfo->nrel_ret,
                        occinfo->nret,
                        occinfo->rel_weight,
                        occinfo->nrel_weight,
                        fdbk_stats->num_ret,
                        fdbk_stats->num_rel);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }
    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        out_p->end = 0;
    }

}

