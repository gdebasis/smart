#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/po_tr_eval.c,v 11.2 1993/02/03 16:54:08 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "spec.h"
#include "buf.h"
#include "eval.h"

static SM_BUF internal_output = {0, 0, (char *) 0};


void
print_prec_eval_list_list (eval_list_list, output) 
EVAL_LIST_LIST *eval_list_list;
SM_BUF *output;
{
    long i, num_query;
    char temp_buf[PATH_LEN];
    SM_BUF *out_p;
    EVAL_LIST *eval_list = eval_list_list->eval_list;

    if (output == NULL) {
        out_p = &internal_output;
        out_p->end = 0;
    }
    else
        out_p = output;

    if (UNDEF == add_buf_string (eval_list_list->description, out_p))
        return;
    if (UNDEF == add_buf_string ("\n Average precision over all recall points for each query\n\n", out_p))
        return;

    for (i = 0; i < eval_list_list->num_eval_list; i++) {
        sprintf (temp_buf, "%3ld. %s\n", i, eval_list[i].description);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }
    if (UNDEF == add_buf_string ("\n", out_p))
        return;
    if (UNDEF == add_buf_string
        ("\nRun:  ", out_p))
        return;
    for (i = 0; i < eval_list_list->num_eval_list; i++) {
        (void) sprintf (temp_buf, "   %2ld    ", i);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }
 
    for (num_query = 0; num_query < eval_list[0].num_eval; num_query++) {
        (void) sprintf (temp_buf, "\n%4ld   ",
                        eval_list[0].eval[num_query].qid);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
        for (i = 0; i < eval_list_list->num_eval_list; i++) {
            (void) sprintf (temp_buf, "%6.4f   ",
                            eval_list[i].eval[num_query].av_recall_precis);
            if (UNDEF == add_buf_string (temp_buf, out_p))
                return;
        }
    }

    if (UNDEF == add_buf_string ("\n", out_p))
        return;
    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        out_p->end = 0;
    }
}
