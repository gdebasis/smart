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
print_eval (eval, output) 
EVAL *eval;
SM_BUF *output;
{
    EVAL_LIST eval_list;
    eval_list.num_eval = 1;
    eval_list.description = NULL;
    eval_list.eval = eval;

    print_eval_list (&eval_list, output);
}

void
print_eval_list (eval_list, output) 
EVAL_LIST *eval_list;
SM_BUF *output;
{
    long i,j;
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

    if (eval_list->description != NULL) {
        if (UNDEF == add_buf_string (eval_list->description, out_p))
            return;
    }

    if (UNDEF == add_buf_string
        ("\nRun number:      ", out_p))
        return;
    for (i = 0; i < num_runs; i++) {
        (void) sprintf (temp_buf, "   %2ld    ", i);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }
    /* Print total numbers retrieved/rel for all runs */
    if (UNDEF == add_buf_string("\nNum_queries:  ", out_p))
        return;
    for (i = 0; i < num_runs; i++) {
        (void) sprintf (temp_buf, "    %5ld", eval[i].qid);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }
    if (UNDEF == add_buf_string("\nTotal number of documents over all queries",
                                out_p))
        return;
    if (UNDEF == add_buf_string("\n    Retrieved:", out_p))
        return;
    for (i = 0; i < num_runs; i++) {
        (void) sprintf (temp_buf, "    %5ld", eval[i].num_ret);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }
    if (UNDEF == add_buf_string("\n    Relevant: ", out_p))
        return;
    for (i = 0; i < num_runs; i++) {
        (void) sprintf (temp_buf, "    %5ld", eval[i].num_rel);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }
    if (UNDEF == add_buf_string("\n    Rel_ret:  ", out_p))
        return;
    for (i = 0; i < num_runs; i++) {
        (void) sprintf (temp_buf, "    %5ld", eval[i].num_rel_ret);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }

    /* Print recall precision figures at eval->num_rp_pts recall levels */
    if (UNDEF == add_buf_string
        ("\nInterpolated Recall - Precision Averages:", out_p))
        return;
    for (j = 0; j < eval->num_rp_pts; j++) {
        (void) sprintf (temp_buf, "\n    at %4.2f     ",
                        (float) j / (eval->num_rp_pts - 1));
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
        for (i = 0; i < num_runs; i++) {
            (void) sprintf (temp_buf, "  %6.4f ",eval[i].int_recall_precis[j]);
            if (UNDEF == add_buf_string (temp_buf, out_p))
                return;
        }
    }

    /* Print average recall precision and percentage improvement */
    (void) sprintf (temp_buf,
                   "\nAverage precision (non-interpolated) over all rel docs\n                ");
    if (UNDEF == add_buf_string (temp_buf, out_p))
        return;
    for (i = 0; i < num_runs; i++) {
        (void) sprintf (temp_buf, "  %6.4f ", eval[i].av_recall_precis);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }
    if (num_runs > 1) {
        (void) sprintf (temp_buf, "\n    %% Change:           ");
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
	if (eval[0].av_recall_precis)
	    for (i = 1; i < num_runs; i++) {
		(void) sprintf (temp_buf, "  %6.1f ",
				((eval[i].av_recall_precis /
				  eval[0].av_recall_precis)
				 - 1.0) * 100.0);
                if (UNDEF == add_buf_string (temp_buf, out_p))
                    return;
            }
	else {
            for (i = 1; i < num_runs; i++) {
                (void) sprintf (temp_buf, "   NaN   ");
                if (UNDEF == add_buf_string (temp_buf, out_p))
                    return;
            }
        }
    }

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

    (void) sprintf (temp_buf, "\nR-Precision (precision after R (= num_rel for a query) docs retrieved):\n    Exact:     ");
    if (UNDEF == add_buf_string (temp_buf, out_p))
        return;
    for (i = 0; i < num_runs; i++) {
        (void) sprintf (temp_buf, "   %6.4f", eval[i].R_recall_precis);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }

    (void) sprintf (temp_buf, "\n\n----------------------------------------------------------------\nThe following measures are tentative for TREC-2\n");
    if (UNDEF == add_buf_string (temp_buf, out_p))
        return;

    (void) sprintf (temp_buf, "\nR-based-Precision (precision after given multiple of R docs retrieved):\n    Exact:     ");
    if (UNDEF == add_buf_string (temp_buf, out_p))
        return;
    for (i = 0; i < num_runs; i++) {
        (void) sprintf (temp_buf, "   %6.4f", eval[i].R_recall_precis);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }
    for (j = 1; j < eval->num_prec_pts; j++) {
        (void) sprintf (temp_buf, "\n    At %4.2f  R:",
                        (float) MAX_RPREC * j / (float) (eval->num_prec_pts - 1));
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
        for (i = 0; i < num_runs; i++) {
            (void) sprintf (temp_buf, "   %6.4f", eval[i].R_prec_cut[j]);
            if (UNDEF == add_buf_string (temp_buf, out_p))
                return;
        }
    }


    (void) sprintf (temp_buf, "\n\n----------------------------------------------------------------\nThe following measures are possible for future TRECs\n");
    if (UNDEF == add_buf_string (temp_buf, out_p))
        return;


    (void) sprintf (temp_buf, "\nRelative Precision:\n   Exact:      ");
    if (UNDEF == add_buf_string (temp_buf, out_p))
        return;
    for (i = 0; i < num_runs; i++) {
        (void) sprintf (temp_buf, "   %6.4f", eval[i].exact_rel_precis);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }
    for (j = 0; j < eval->num_cutoff; j++) {
        (void) sprintf (temp_buf, "\n   At %3ld docs:", eval->cutoff[j]);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
        for (i = 0; i < num_runs; i++) {
            (void) sprintf (temp_buf, "   %6.4f", eval[i].rel_precis_cut[j]);
            if (UNDEF == add_buf_string (temp_buf, out_p))
                return;
        }
    }

    (void) sprintf (temp_buf, "\nAverage precision for first R docs retrieved:\n               ");
    if (UNDEF == add_buf_string (temp_buf, out_p))
        return;
    for (i = 0; i < num_runs; i++) {
        (void) sprintf (temp_buf, "   %6.4f", eval[i].av_R_precis);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }

    (void) sprintf (temp_buf, "\nFallout - Recall Averages (recall after X nonrel docs retrieved):");
    if (UNDEF == add_buf_string (temp_buf, out_p))
        return;
    for (j = 0; j < eval->num_fr_pts; j++) {
        (void) sprintf (temp_buf, "\n    At %3ld docs:",
                        (long) (eval->max_fall_ret * j) / (eval->num_fr_pts - 1));
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
        for (i = 0; i < num_runs; i++) {
            (void) sprintf (temp_buf, "   %6.4f", eval[i].fall_recall[j]);
            if (UNDEF == add_buf_string (temp_buf, out_p))
                return;
        }
    }
    (void) sprintf (temp_buf, "\nAverage recall for first %ld nonrel docs retrieved:\n                ", eval->max_fall_ret);
    if (UNDEF == add_buf_string (temp_buf, out_p))
        return;
    for (i = 0; i < num_runs; i++) {
        (void) sprintf (temp_buf, "   %6.4f", eval[i].av_fall_recall);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }

    (void) sprintf (temp_buf, "\n\n----------------------------------------------------------------\nThe following measures are interpolated versions of measures above.\nFor the following, interpolated_prec(X) == MAX (prec(Y)) for all Y >= X\nAll these measures are experimental\n");
    if (UNDEF == add_buf_string (temp_buf, out_p))
        return;

    /* Print average recall precision and percentage improvement */
    (void) sprintf (temp_buf,
                   "\nAverage interpolated precision over all rel docs\n                ");
    if (UNDEF == add_buf_string (temp_buf, out_p))
        return;
    for (i = 0; i < num_runs; i++) {
        (void) sprintf (temp_buf, "  %6.4f ", eval[i].int_av_recall_precis);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }
    if (num_runs > 1) {
        (void) sprintf (temp_buf, "\n    %% Change:           ");
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
        for (i = 1; i < num_runs; i++) {
            (void) sprintf (temp_buf, "  %6.1f ",
                            ((eval[i].int_av_recall_precis /
                              eval[0].int_av_recall_precis)
                             - 1.0) * 100.0);
            if (UNDEF == add_buf_string (temp_buf, out_p))
                return;
        }
    }

    (void) sprintf (temp_buf, "\nR-based-interpolated-Precision:\n    Exact:     ");
    if (UNDEF == add_buf_string (temp_buf, out_p))
        return;
    for (i = 0; i < num_runs; i++) {
        (void) sprintf (temp_buf, "   %6.4f", eval[i].int_R_recall_precis);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }
    for (j = 1; j < eval->num_prec_pts; j++) {
        (void) sprintf (temp_buf, "\n    At %4.2f  R:",
                        (float) MAX_RPREC * j / (float) (eval->num_prec_pts - 1));
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
        for (i = 0; i < num_runs; i++) {
            (void) sprintf (temp_buf, "   %6.4f", eval[i].int_R_prec_cut[j]);
            if (UNDEF == add_buf_string (temp_buf, out_p))
                return;
        }
    }

    (void) sprintf (temp_buf, "\nAverage interpolated precision for first R docs retrieved:\n               ");
    if (UNDEF == add_buf_string (temp_buf, out_p))
        return;
    for (i = 0; i < num_runs; i++) {
        (void) sprintf (temp_buf, "   %6.4f", eval[i].int_av_R_precis);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }


    (void) sprintf (temp_buf, "\n\n----------------------------------------------------------------\nThe following measures included for TREC 1 compatability\n");
    if (UNDEF == add_buf_string (temp_buf, out_p))
        return;

    (void) sprintf (temp_buf, "\nPrecision:\n   Exact:      ");
    if (UNDEF == add_buf_string (temp_buf, out_p))
        return;
    for (i = 0; i < num_runs; i++) {
        (void) sprintf (temp_buf, "   %6.4f", eval[i].exact_precis);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }
    (void) sprintf (temp_buf, "\nRecall:\n   Exact:      ");
    if (UNDEF == add_buf_string (temp_buf, out_p))
        return;
    for (i = 0; i < num_runs; i++) {
        (void) sprintf (temp_buf, "   %6.4f", eval[i].exact_recall);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }
    for (j = 0; j < eval->num_cutoff; j++) {
        (void) sprintf (temp_buf, "\n   at %3ld docs:", eval->cutoff[j]);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
        for (i = 0; i < num_runs; i++) {
            (void) sprintf (temp_buf, "   %6.4f", eval[i].recall_cut[j]);
            if (UNDEF == add_buf_string (temp_buf, out_p))
                return;
        }
    }

    /* Print average recall precision and percentage improvement */
    (void) sprintf (temp_buf,
                    "\nAverage interpolated precision for all %ld recall points\n   %2ld-pt Avg:   ",
                    eval->num_rp_pts, eval->num_rp_pts);
    if (UNDEF == add_buf_string (temp_buf, out_p))
        return;
    for (i = 0; i < num_runs; i++) {
        (void) sprintf (temp_buf, "  %6.4f ",eval[i].int_nrp_av_recall_precis);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }
    if (num_runs > 1) {
        (void) sprintf (temp_buf, "\n    %% Change:           ");
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
        for (i = 1; i < num_runs; i++) {
            (void) sprintf (temp_buf, "  %6.1f ",
                            ((eval[i].int_nrp_av_recall_precis /
                              eval[0].int_nrp_av_recall_precis)
                             - 1.0) * 100.0);
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
