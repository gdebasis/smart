#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/po_ind_tr.c,v 11.2 1993/02/03 16:54:06 smart Exp $";
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
#include "io.h"
#include "buf.h"
#include "tr_vec.h"
#include "rr_vec.h"
#include "rel_header.h"

static char *tr_file;
static char *qrels_file;
static char *run_name;

static SPEC_PARAM spec_args[] = {
    {"print.tr_file",         getspec_dbfile, (char *) &tr_file},
    {"print.qrels_file",      getspec_dbfile, (char *) &qrels_file},
    {"print.run_name",        getspec_string, (char *) &run_name},
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static SM_BUF internal_output = {0, 0, (char *) 0};

void
print_ind_tr (spec_list, output)
SPEC_LIST *spec_list;
SM_BUF *output;
{
    long i,j;
    long num_runs;
    int *tr_fd;
    int qrels_fd;
    REL_HEADER *rh;
    long qid, max_qid;
    TR_VEC tr_vec;
    RR_VEC qrels_vec;
    long num_good_runs;
    long num_rel;
    SM_BUF *out_p;
    char temp_buf[PATH_LEN];
    long max_iter, num_max_iter;

    if (spec_list == NULL || spec_list->num_spec <= 0)
        return;

    if (output == NULL) {
        out_p = &internal_output;
        out_p->end = 0;
    }
    else {
        out_p = output;
    }

    /* Reserve space for each tr file descriptor */
    if (NULL == (tr_fd = (int *) 
                 malloc ((unsigned) spec_list->num_spec * sizeof (int))))
        return;

    num_runs = 0;
    qrels_fd = UNDEF;
    /* Get each tr_file in turn */
    for (i = 0; i < spec_list->num_spec; i++) {
        if (UNDEF == lookup_spec (spec_list->spec[i],
                                  &spec_args[0],
                                  num_spec_args))
            return;
        if (! VALID_FILE (tr_file) ||
            UNDEF == (tr_fd[num_runs] =
                      open_tr_vec (tr_file, (long) SRDONLY))) {
            (void) sprintf(temp_buf,
                           "Run %s cannot be evaluated. Ignored\n",
                           tr_file);
            if (UNDEF == add_buf_string (temp_buf, out_p))
                return;
            continue;
        }
        if (qrels_fd == UNDEF && VALID_FILE (qrels_file))
            qrels_fd = open_rr_vec (qrels_file, (long) SRDONLY);
        num_runs++;
        (void) sprintf (temp_buf,
                        "%ld. %s\n",
                        num_runs, 
                        VALID_FILE (run_name) ? run_name : tr_file);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }

    if (num_runs == 0) {
        (void)add_buf_string ("\n***ERROR*** No valid runs included\n", out_p);
        return;
    }

    if (UNDEF == add_buf_string ("\nQuery\tNum_rel\t  Rel_retrieved/retrieved",
                                 out_p))
        return;

    if (NULL == (rh = get_rel_header (tr_file)))
        return;
    max_qid = rh->max_primary_value;
    
    for (qid = 0; qid <= max_qid; qid++) {
        qrels_vec.qid = qid;
        (void) sprintf (temp_buf, "\n%ld", qid);
        if (qrels_fd == UNDEF ||
            1 != seek_rr_vec (qrels_fd, &qrels_vec) ||
            1 != read_rr_vec (qrels_fd, &qrels_vec)) {
            (void) strcpy (&temp_buf[strlen(temp_buf)], "\t------");
        }
        else 
            (void) sprintf (&temp_buf[strlen(temp_buf)],
                            "\t%ld", qrels_vec.num_rr);
        num_good_runs = 0;
        for (i = 0; i < num_runs; i++) {
            tr_vec.qid = qid;
            if (1 != seek_tr_vec (tr_fd[i], &tr_vec) ||
                1 != read_tr_vec (tr_fd[i], &tr_vec)) {
                (void) strcpy (&temp_buf[strlen(temp_buf)], "\t------");
            }
            else {
                /* Throw out docs occurring in previous iterations */
                for (j = 0, max_iter = 0; j < tr_vec.num_tr; j++) {
                    if (tr_vec.tr[j].iter > max_iter)
                        max_iter = tr_vec.tr[j].iter;
                }
                num_max_iter = num_rel = 0;
                for (j = 0; j < tr_vec.num_tr; j++) {
                    if (tr_vec.tr[j].iter == max_iter) {
                        num_max_iter++;
                        if (tr_vec.tr[j].rel) {
                            num_rel++;
                        }
                    }
                }
                (void) sprintf (&temp_buf[strlen(temp_buf)],
                                "\t%ld/%ld", num_rel, num_max_iter);
                num_good_runs++;
            }
        }
        if (num_good_runs) {
            if (UNDEF == add_buf_string (temp_buf, out_p))
                return;
        }
    }

    for (i = 0; i < num_runs; i++) {
        if (UNDEF == close_tr_vec (tr_fd[i]))
            return;
    }
    (void) free ((char *) tr_fd);
    if (qrels_fd != UNDEF && UNDEF == close_rr_vec (qrels_fd))
        return;
    
    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        out_p->end = 0;
    }

    return;
}

