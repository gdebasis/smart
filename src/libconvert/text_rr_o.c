#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/text_rr_o.c,v 11.2 1993/02/03 16:49:26 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Take an entire text file, and add its tokens to rr file
 *1 convert.obj.text_rr_obj
 *2 text_rr_obj (text_file, rr_file, inst)
 *3 char *text_file;
 *3 char *rr_file;
 *3 int inst;

 *4 init_text_rr_obj (spec, unused)
 *5   "text_rr_obj.rr_file"
 *5   "text_rr_obj.rr_file.rwmode"
 *5   "text_rr_obj.trace"
 *4 close_text_rr_obj (inst)

 *7 Read text rr tuples (rr stands for relevant rank) from text_file,
 *7 convert to RR_VEC and write to the RR_VEC relational object rr_file.
 *7 If text_file is NULL or non-valid (eg "-"), then stdin is used.
 *7 If rr_file is NULL, then value of rr_file spec parameter is used.
 *7 Format of text file is lines of the form
 *7    qid  did  rank  sim
 *7 giving the rank and similarity of relevant document did to query qid.
 *7 Input must be sorted by qid.
 *7 If rank and sim do not appear on the input line, they are set to 0.
 *7 Return UNDEF if error, else 1;
***********************************************************************/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "io.h"
#include "functions.h"
#include "spec.h"
#include "trace.h"
#include "smart_error.h"
#include "rr_vec.h"
#include "textline.h"


static char *default_rr_file;
static long rr_mode;

static SPEC_PARAM spec_args[] = {
    {"text_rr_obj.rr_file",        getspec_dbfile, (char *) &default_rr_file},
    {"text_rr_obj.rr_file.rwmode", getspec_filemode, (char *) &rr_mode},
    TRACE_PARAM ("text_rr_obj.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int comp_did();

int
init_text_rr_obj (spec, unused)
SPEC *spec;
char *unused;
{
    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_text_rr_obj");
    PRINT_TRACE (2, print_string, "Trace: leaving init_text_rr_obj");

    return (0);
}

int
text_rr_obj (text_file, rr_file, inst)
char *text_file;
char *rr_file;
int inst;
{
    RR_VEC rr_vec;
    unsigned rr_size;       /* Number of RR_TUP elements space allocated */

    FILE *in_fd;
    int rr_fd;
    char line_buf[PATH_LEN];
    SM_TEXTLINE textline;
    char *token_array[4];

    long qid;

    PRINT_TRACE (2, print_string, "Trace: entering text_rr_obj");

    if (VALID_FILE (text_file)) {
        if (NULL == (in_fd = fopen (text_file, "r")))
            return (UNDEF);
    }
    else
        in_fd = stdin;

    if (rr_file == (char *) NULL)
        rr_file = default_rr_file;

    if (UNDEF == (rr_fd = open_rr_vec (rr_file, rr_mode))) {
        clr_err();
        if (UNDEF == (rr_fd = open_rr_vec (rr_file, rr_mode | SCREATE)))
            return (UNDEF);
    }

    rr_vec.qid = 0;
    rr_vec.num_rr = 0;
    rr_size = 100;
    if (NULL == (rr_vec.rr = (RR_TUP *) malloc (rr_size * sizeof (RR_TUP))))
        return (UNDEF);
    
    textline.max_num_tokens = 4;
    textline.tokens = token_array;
    
    while (NULL != fgets (line_buf, PATH_LEN, in_fd)) {
        /* Break line_buf up into tokens */
        (void) text_textline (line_buf, &textline);
        if (textline.num_tokens < 2) {
            set_error (SM_ILLPA_ERR, "Illegal input line", "text_rr_obj");
            return (UNDEF);
        }
        qid = atol (textline.tokens[0]);
        if (qid != rr_vec.qid) {
            /* Output previous vector */
            if (rr_vec.qid != 0) {
                (void) qsort ((char *) rr_vec.rr,
                              (int) rr_vec.num_rr,
                              sizeof (RR_TUP),
                              comp_did);
                if (UNDEF == seek_rr_vec (rr_fd, &rr_vec) ||
                    UNDEF == write_rr_vec (rr_fd, &rr_vec))
                    return (UNDEF);
            }
            rr_vec.qid = qid;
            rr_vec.num_rr = 0;
        }
        rr_vec.rr[rr_vec.num_rr].did = atol (textline.tokens[1]);
        rr_vec.rr[rr_vec.num_rr].rank = 0;
        rr_vec.rr[rr_vec.num_rr].sim = 0.0;
        if (textline.num_tokens >= 3) {
            rr_vec.rr[rr_vec.num_rr].rank = atol (textline.tokens[2]);
            if (textline.num_tokens == 4)
                rr_vec.rr[rr_vec.num_rr].sim = atof (textline.tokens[3]);
        }
        rr_vec.num_rr++;
        if (rr_vec.num_rr >= rr_size) {
            rr_size += rr_size;
            if (NULL == (rr_vec.rr = (RR_TUP *)
                    realloc ((char *) rr_vec.rr, rr_size * sizeof (RR_TUP))))
                return (UNDEF);
        }
    }

    /* Output previous vector */
    if (rr_vec.qid != 0) {
        (void) qsort ((char *) rr_vec.rr,
                      (int) rr_vec.num_rr,
                      sizeof (RR_TUP),
                      comp_did);
        if (UNDEF == seek_rr_vec (rr_fd, &rr_vec) ||
            UNDEF == write_rr_vec (rr_fd, &rr_vec))
            return (UNDEF);
    }

    (void) free ((char *) rr_vec.rr);

    if (UNDEF == close_rr_vec (rr_fd)) {
        return (UNDEF);
    }

    if (VALID_FILE (text_file))
        (void) fclose (in_fd);

    PRINT_TRACE (2, print_string, "Trace: leaving text_rr_obj");
    return (1);
}

int
close_text_rr_obj (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_text_rr_obj");
    PRINT_TRACE (2, print_string, "Trace: leaving close_text_rr_obj");
    
    return (0);
}

static int comp_did (rr1, rr2)
RR_TUP *rr1;
RR_TUP *rr2;
{
    return (rr1->did - rr2->did);
}


