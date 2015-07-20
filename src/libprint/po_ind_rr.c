#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/po_ind_rr.c,v 11.2 1993/02/03 16:54:06 smart Exp $";
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
#include "rr_vec.h"
#include "rel_header.h"

static char *rr_file;
static char *qrels_file;
static char *run_name;

static SPEC_PARAM spec_args[] = {
    {"print.rr_file",         getspec_dbfile, (char *) &rr_file},
    {"print.qrels_file",      getspec_dbfile, (char *) &qrels_file},
    {"print.run_name",        getspec_string, (char *) &run_name},
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static SM_BUF internal_output = {0, 0, (char *) 0};

static int save_rrvec();
static void free_rrvec();

void
print_ind_rr (spec_list, output)
SPEC_LIST *spec_list;
SM_BUF *output;
{
    long i;
    int num_runs;
    int *rr_fd;
    RR_VEC *rr_vec;
    int *rr_index;
    int qrels_fd;
    RR_VEC qrels_vec;
    int qrels_index;
    REL_HEADER *rh;
    long qid, max_qid;
    SM_BUF *out_p;
    char temp_buf[PATH_LEN];

    if (spec_list == NULL || spec_list->num_spec <= 0)
        return;

    if (output == NULL) {
        out_p = &internal_output;
        out_p->end = 0;
    }
    else {
        out_p = output;
    }

    /* Reserve space for each rr file descriptor, rr_vec, and rr_index */
    if (NULL == (rr_fd = (int *) 
                 malloc ((unsigned) spec_list->num_spec * sizeof (int))) ||
        NULL == (rr_vec = (RR_VEC *) 
                 malloc ((unsigned) spec_list->num_spec * sizeof (RR_VEC))) ||
        NULL == (rr_index = (int *) 
                 malloc ((unsigned) spec_list->num_spec * sizeof (int))))
        return;

    num_runs = 0;
    qrels_fd = UNDEF;
    max_qid = 0;
    /* Get each rr_file in turn */
    for (i = 0; i < spec_list->num_spec; i++) {
        if (UNDEF == lookup_spec (spec_list->spec[i],
                                  &spec_args[0],
                                  num_spec_args))
            return;
        if (! VALID_FILE (rr_file) ||
            UNDEF == (rr_fd[num_runs] =
                      open_rr_vec (rr_file, (long) SRDONLY))) {
            (void) sprintf(temp_buf,
                           "Run %s cannot be evaluated. Ignored\n",
                           rr_file);
            if (UNDEF == add_buf_string (temp_buf, out_p))
                return;
            continue;
        }

        if (NULL == (rh = get_rel_header (rr_file)))
            return;
        max_qid = MAX (max_qid, rh->max_primary_value);

        if (qrels_fd == UNDEF && VALID_FILE (qrels_file))
            qrels_fd = open_rr_vec (qrels_file, (long) SRDONLY);

        num_runs++;
        (void) sprintf (temp_buf,
                        "%d. %s\n",
                        num_runs, 
                        VALID_FILE (run_name) ? run_name : rr_file);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }

    if (num_runs == 0) {
        (void)add_buf_string ("\n***ERROR*** No valid runs included\n", out_p);
        return;
    }
    if (qrels_fd == UNDEF) {
        (void) add_buf_string ("\n***ERROR*** No qrels file\n", out_p);
        return;
    }

    if (UNDEF == add_buf_string ("\nQuery\tDoc_id\tRanks of rel docs", out_p))
        return;

    for (qid = 0; qid <= max_qid; qid++) {
        qrels_vec.qid = qid;
        if (1 != seek_rr_vec (qrels_fd, &qrels_vec) ||
            1 != read_rr_vec (qrels_fd, &qrels_vec))
            continue;
        for (i = 0; i < num_runs; i++) {
            rr_vec[i].qid = qid;
            if (1 != seek_rr_vec (rr_fd[i], &rr_vec[i]) ||
                1 != read_rr_vec (rr_fd[i], &rr_vec[i]))
                rr_vec[i].num_rr = 0;
            if (UNDEF == save_rrvec (&rr_vec[i]))
                return;
            rr_index[i] = 0;
        }
        for (qrels_index = 0; qrels_index < qrels_vec.num_rr; qrels_index++) {
            (void) sprintf (temp_buf,
                            "\n%ld\t%3ld :",
                            qid, qrels_vec.rr[qrels_index].did);
            for (i = 0; i < num_runs; i++) {
                if (rr_index[i] < rr_vec[i].num_rr &&
                    rr_vec[i].rr[rr_index[i]].did ==
                    qrels_vec.rr[qrels_index].did){
                    (void) sprintf (&temp_buf[strlen(temp_buf)],
                                    "\t%ld", rr_vec[i].rr[rr_index[i]].rank);
                    rr_index[i]++;
                }
                else
                    (void) strcpy (&temp_buf[strlen(temp_buf)], "\t-----");
            }
            if (UNDEF == add_buf_string (temp_buf, out_p))
                return;
        }
        for (i = 0; i < num_runs; i++) {
            free_rrvec (&rr_vec[i]);
        }
    }

    for (i = 0; i < num_runs; i++) {
        if (UNDEF == close_rr_vec (rr_fd[i]))
            return;
    }
    (void) free ((char *) rr_fd);
    (void) free ((char *) rr_vec);
    (void) free ((char *) rr_index);
    if (UNDEF == close_rr_vec (qrels_fd))
        return;
    
    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        out_p->end = 0;
    }

    return;
}


static int
save_rrvec (rr_vec)
RR_VEC *rr_vec;
{
    RR_TUP *rr_tup;

    if (rr_vec->num_rr == 0)
        return (0);
    if (NULL == (rr_tup = (RR_TUP *)
                 malloc ((unsigned) rr_vec->num_rr * sizeof (RR_TUP))))
        return (UNDEF);
    bcopy ((char *) rr_vec->rr,
           (char *) rr_tup, 
           (int) rr_vec->num_rr * sizeof (RR_TUP));
    rr_vec->rr = rr_tup;
    return (1);
}

static void
free_rrvec (rr_vec)
RR_VEC *rr_vec;
{
    if (rr_vec->num_rr > 0)
        (void) free ((char *) rr_vec->rr);
}
