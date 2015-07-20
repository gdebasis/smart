#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/text_tr_o.c,v 11.2 1993/02/03 16:49:26 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Take an entire top rank text file, and add its tokens to tr file
 *1 convert.obj.text_tr_obj
 *2 text_tr_obj (text_file, tr_file, inst)
 *3 char *text_file;
 *3 char *tr_file;
 *3 int inst;

 *4 init_text_tr_obj (spec, unused)
 *5   "text_tr_obj.tr_file"
 *5   "text_tr_obj.tr_file.rwmode"
 *5   "text_tr_obj.trace"
 *4 close_text_tr_obj (inst)

 *7 Read text tr tuples (tr stands for top rank) from text_file,
 *7 convert to TR_VEC and write to the TR_VEC relational object tr_file.
 *7 If text_file is NULL or non-valid (eg "-"), then stdin is used.
 *7 If tr_file is NULL, then value of tr_file spec parameter is used.
 *7 Format of text file is lines of the form
 *7        qid  did  rank [rel [action [iter [sim]]]]
 *7 giving the rank, similarity, relevance of document did to query qid and
 *7 what has been done with the document in the past.
 *7 The first three fields must appear on every line, the other 4 are set to
 *7 0 if they do not appear.
 *7 Qid and did and rank must be positive.
 *7 Input must be sorted by qid.
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
#include "tr_vec.h"
#include "textline.h"


static char *default_tr_file;
static long tr_mode;

static SPEC_PARAM spec_args[] = {
    {"text_tr_obj.tr_file",        getspec_dbfile, (char *) &default_tr_file},
    {"text_tr_obj.tr_file.rwmode", getspec_filemode, (char *) &tr_mode},
    TRACE_PARAM ("text_tr_obj.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int comp_did();


int
init_text_tr_obj (spec, unused)
SPEC *spec;
char *unused;
{
    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_text_tr_obj");
    PRINT_TRACE (2, print_string, "Trace: leaving init_text_tr_obj");

    return (0);
}

int
text_tr_obj (text_file, tr_file, inst)
char *text_file;
char *tr_file;
int inst;
{
    TR_VEC tr_vec;
    unsigned tr_size;       /* Number of TR_TUP elements space allocated */

    FILE *in_fd;
    int tr_fd;
    char line_buf[PATH_LEN];
    SM_TEXTLINE textline;
    char *token_array[7];

    long qid;

    PRINT_TRACE (2, print_string, "Trace: entering text_tr_obj");

    if (VALID_FILE (text_file)) {
        if (NULL == (in_fd = fopen (text_file, "r")))
            return (UNDEF);
    }
    else
        in_fd = stdin;

    if (tr_file == (char *) NULL)
        tr_file = default_tr_file;

    if (UNDEF == (tr_fd = open_tr_vec (tr_file, tr_mode))) {
        clr_err();
        if (UNDEF == (tr_fd = open_tr_vec (tr_file, tr_mode | SCREATE)))
            return (UNDEF);
    }

    tr_vec.qid = 0;
    tr_vec.num_tr = 0;
    tr_size = 100;
    if (NULL == (tr_vec.tr = (TR_TUP *) malloc (tr_size * sizeof (TR_TUP))))
        return (UNDEF);

    textline.max_num_tokens = 7;
    textline.tokens = token_array;
    
    while (NULL != fgets (line_buf, PATH_LEN, in_fd)) {
        /* Break line_buf up into tokens */
        (void) text_textline (line_buf, &textline);
        if (textline.num_tokens < 3) {
            set_error (SM_ILLPA_ERR, "Illegal input line", "text_tr_obj");
            return (UNDEF);
        }
        qid = atol (textline.tokens[0]);
        if (qid != tr_vec.qid) {
            /* Output previous vector */
            if (tr_vec.qid != 0) {
                (void) qsort ((char *) tr_vec.tr,
                              (int) tr_vec.num_tr,
                              sizeof (TR_TUP),
                              comp_did);
                if (UNDEF == seek_tr_vec (tr_fd, &tr_vec) ||
                    UNDEF == write_tr_vec (tr_fd, &tr_vec))
                    return (UNDEF);
            }
            tr_vec.qid = qid;
            tr_vec.num_tr = 0;
        }
        tr_vec.tr[tr_vec.num_tr].did = atol (textline.tokens[1]);
        tr_vec.tr[tr_vec.num_tr].rank = atol (textline.tokens[2]);
        tr_vec.tr[tr_vec.num_tr].action = '\0';
        tr_vec.tr[tr_vec.num_tr].rel = '\0';
        tr_vec.tr[tr_vec.num_tr].iter = 0;
        tr_vec.tr[tr_vec.num_tr].sim = 0.0;
        if (textline.num_tokens >= 4) {
            tr_vec.tr[tr_vec.num_tr].rel = atol (textline.tokens[3]);
            if (textline.num_tokens >= 5)
                tr_vec.tr[tr_vec.num_tr].action = atol (textline.tokens[4]);
            if (textline.num_tokens >= 6)
                tr_vec.tr[tr_vec.num_tr].iter = atol (textline.tokens[5]);
            if (textline.num_tokens >= 7)
                tr_vec.tr[tr_vec.num_tr].sim = atof (textline.tokens[6]);
        }
        tr_vec.num_tr++;
        if (tr_vec.num_tr >= tr_size) {
            tr_size += tr_size;
            if (NULL == (tr_vec.tr = (TR_TUP *)
                    realloc ((char *) tr_vec.tr, tr_size * sizeof (TR_TUP))))
                return (UNDEF);
        }
    }

    /* Output previous vector */
    if (tr_vec.qid != 0) {
        (void) qsort ((char *) tr_vec.tr,
                      (int) tr_vec.num_tr,
                      sizeof (TR_TUP),
                      comp_did);
        if (UNDEF == seek_tr_vec (tr_fd, &tr_vec) ||
            UNDEF == write_tr_vec (tr_fd, &tr_vec))
            return (UNDEF);
    }

    (void) free ((char *) tr_vec.tr);

    if (UNDEF == close_tr_vec (tr_fd)) {
        return (UNDEF);
    }

    if (VALID_FILE (text_file))
        (void) fclose (in_fd);

    PRINT_TRACE (2, print_string, "Trace: leaving text_tr_obj");
    return (1);
}

int
close_text_tr_obj (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_text_tr_obj");
    PRINT_TRACE (2, print_string, "Trace: leaving close_text_tr_obj");
    
    return (0);
}

static int comp_did (tr1, tr2)
TR_TUP *tr1;
TR_TUP *tr2;
{
    if (tr1->did < tr2->did)	/* explicit compare needed because */
	return -1;		/* of strange did's that show up */
    if (tr1->did > tr2->did)	/* (suppose dids both are -MAXINT) */
	return 1;

    return 0;
}
