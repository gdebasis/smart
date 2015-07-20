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
print_prec_eval (eval, output) 
EVAL *eval;
SM_BUF *output;
{
    EVAL_LIST eval_list;
    eval_list.num_eval = 1;
    eval_list.description = NULL;
    eval_list.eval = eval;

    print_prec_eval_list (&eval_list, output);
}


void
print_prec_eval_list (eval_list, output) 
EVAL_LIST *eval_list;
SM_BUF *output;
{
    long i, j;
    char temp_buf[PATH_LEN];
    SM_BUF *out_p;
    long num_runs = eval_list->num_eval;
    EVAL *eval = eval_list->eval;

    if (output == NULL) {
        out_p = &internal_output;
        out_p->end = 0;
    }
    else
        out_p = output;

    for (i = 0; i < num_runs; i++) {
        (void) sprintf (temp_buf, "%6.4f   ", eval[i].av_recall_precis);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;

	(void) sprintf (temp_buf, "\nPrecision:");
	if (UNDEF == add_buf_string (temp_buf, out_p))
	    return;
	for (j = 0; j < eval->num_cutoff; j++) {
	    (void) sprintf (temp_buf, "\n   At %3ld docs:", eval->cutoff[j]);
	    if (UNDEF == add_buf_string (temp_buf, out_p))
		return;
	    for (i = 0; i < num_runs; i++) {
		(void) sprintf (temp_buf, "   %6.4f", eval[i].precis_cut[j]);
		if (UNDEF == add_buf_string (temp_buf, out_p))
		    return;
	    }
	}
    }
    if (UNDEF == add_buf_string ("\n", out_p))
        return;

    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        out_p->end = 0;
    }
}
