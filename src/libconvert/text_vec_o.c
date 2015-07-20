#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/text_vec_o.c,v 11.2 1993/02/03 16:49:21 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Take an entire text file, and add its tokens to vec file
 *1 convert.obj.text_vec_obj
 *2 text_vec_obj (text_file, vec_file, inst)
 *3 char *text_file;
 *3 char *vec_file;
 *3 int inst;

 *4 init_text_vec_obj (spec, unused)
 *5   "text_vec_obj.vec_file"
 *5   "text_vec_obj.vec_file.rwmode"
 *5   "text_vec_obj.trace"
 *4 close_text_vec_obj (inst)

 *7 Read text vec tuples from text_file,
 *7 convert to VEC format and write to the VEC relational object vec_file.
 *7 If text_file is NULL or non-valid (eg "-"), then stdin is used.
 *7 If vec_file is NULL, then value of vec_file spec parameter is used.
 *7 Format of text file is lines of the form
 *7    did  ctype  con   weight  [optional tokens]
 *7 giving the weight of concept con with ctype ctype occurring in did. 
 *7 Input must be sorted by did and then ctype and then con.
 *7 Optional tokens are ignored.
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
#include "vector.h"
#include "textline.h"


static char *default_vec_file;
static long vec_mode;

static SPEC_PARAM spec_args[] = {
    {"text_vec_obj.doc_file",        getspec_dbfile, (char *) &default_vec_file},
    {"text_vec_obj.doc_file.rwmode", getspec_filemode, (char *) &vec_mode},
    TRACE_PARAM ("text_vec_obj.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

int
init_text_vec_obj (spec, unused)
SPEC *spec;
char *unused;
{
    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_text_vec_obj");
    PRINT_TRACE (2, print_string, "Trace: leaving init_text_vec_obj");

    return (0);
}

int
text_vec_obj (text_file, vec_file, inst)
char *text_file;
char *vec_file;
int inst;
{
    VEC vec;
    unsigned vec_size, ctype_size; /* Number of elements space allocated */

    FILE *in_fd;
    int vec_fd;
    char line_buf[PATH_LEN];
    SM_TEXTLINE textline;
    char *token_array[4];

    long did;
    long old_ctype, ctype;

    long i;
    PRINT_TRACE (2, print_string, "Trace: entering text_vec_obj");

    if (VALID_FILE (text_file)) {
        if (NULL == (in_fd = fopen (text_file, "r")))
            return (UNDEF);
    }
    else
        in_fd = stdin;

    if (vec_file == (char *) NULL)
        vec_file = default_vec_file;

    if (UNDEF == (vec_fd = open_vector (vec_file, vec_mode))) {
        clr_err();
        if (UNDEF == (vec_fd = open_vector (vec_file, vec_mode | SCREATE)))
            return (UNDEF);
    }

    vec.id_num.id = 0;
    EXT_NONE(vec.id_num.ext);
    vec.num_conwt = 0;
    vec.num_ctype = 0;
    old_ctype = -1;
    vec_size = 100;
    if (NULL == (vec.con_wtp = (CON_WT *) malloc (vec_size * sizeof (CON_WT))))
        return (UNDEF);
    ctype_size = 32;
    if (NULL == (vec.ctype_len = (long *) malloc (ctype_size * sizeof (long))))
        return (UNDEF);

    textline.max_num_tokens = 4;
    textline.tokens = token_array;
    
    while (NULL != fgets (line_buf, PATH_LEN, in_fd)) {
        PRINT_TRACE (7, print_string, line_buf);
        /* Break line_buf up into tokens */
        (void) text_textline (line_buf, &textline);
        if (textline.num_tokens < 4) {
            set_error (SM_ILLPA_ERR, "Illegal input line", "text_vec_obj");
            return (UNDEF);
        }
        did = atol (textline.tokens[0]);
        if (did != vec.id_num.id) {
            /* Output previous vector */
            if (vec.id_num.id != 0) {
                PRINT_TRACE (7, print_vector, &vec);
                if (UNDEF == seek_vector (vec_fd, &vec) ||
                    UNDEF == write_vector (vec_fd, &vec))
                    return (UNDEF);
            }
            vec.id_num.id = did;
            vec.num_conwt = 0;
            vec.num_ctype = 0;
            old_ctype = -1;
        }
        ctype = atol (textline.tokens[1]);
        if (old_ctype != ctype) {
            if (ctype >= ctype_size) {
                ctype_size += ctype;
                if (NULL == (vec.ctype_len = (long *)
                             realloc ((char *) vec.ctype_len,
                                      (unsigned) ctype_size * sizeof (long))))
                    return (UNDEF);
            }
            for (i = old_ctype + 1; i <= ctype; i++)
                vec.ctype_len[i] = 0;
            if (ctype + 1 > vec.num_ctype)
                vec.num_ctype = ctype + 1;
            old_ctype = ctype;
        }
        vec.ctype_len[ctype]++;
        
        vec.con_wtp[vec.num_conwt].con = atol (textline.tokens[2]);
        vec.con_wtp[vec.num_conwt].wt  = atof (textline.tokens[3]);
        vec.num_conwt++;
        if (vec.num_conwt >= vec_size) {
            vec_size += vec_size;
            if (NULL == (vec.con_wtp = (CON_WT *)
                    realloc ((char *) vec.con_wtp,
                             (unsigned) vec_size * sizeof (CON_WT))))
                return (UNDEF);
        }
    }

    /* Output last vector */
    if (vec.id_num.id != 0) {
        if (UNDEF == seek_vector (vec_fd, &vec) ||
            UNDEF == write_vector (vec_fd, &vec))
            return (UNDEF);
    }

    if (UNDEF == close_vector (vec_fd)) {
        return (UNDEF);
    }

    if (VALID_FILE (text_file))
        (void) fclose (in_fd);

    PRINT_TRACE (2, print_string, "Trace: leaving text_vec_obj");
    return (1);
}

int
close_text_vec_obj (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_text_vec_obj");
    PRINT_TRACE (2, print_string, "Trace: leaving close_text_vec_obj");
    
    return (0);
}


