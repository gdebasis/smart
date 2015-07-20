#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/text_tr_o.c,v 11.2 1993/02/03 16:49:26 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Take an entire text file composed of float fields, convert to dir_array
 *1 convert.obj.text_da_obj
 *2 text_da_obj (text_file, da_file, inst)
 *3 char *text_file;
 *3 char *da_file;
 *3 int inst;

 *4 init_text_da_obj (spec, unused)
 *5   "text_da_obj.rwmode"
 *5   "text_da_obj.trace"
 *4 close_text_da_obj (inst)

 *7 Read text tuples from text_file, where every line must have the
 *7 same number of fields, and the first field gives the key field
 *7    t1.f1  t1.f2  t1.f3  t1.f4 ... t1.fn
 *7    t2.f1  t2.f2  t2.f3  t2.f4 ... t2.fn
 *7    t3.f1  t3.f2  t3.f3  t3.f4 ... t3.fn
 *7 All fields except the first are read as floats, and stored as float.
 *7 Gather all tuples with same key (input assumed to be sorted by key),
 *7 and write them out in a dir_array tuple.  If t1.f1 == t2.f1 != t3.f1,
 *7 Then the dir_array entry is written
 *7    t1.f2 t1.f3  t1.f4 ... t1.fn t2.f2  t2.f3  t2.f4 ... t2.fn
 *7 with key of t1.f1.
 *7 convert to  and write to the TR_VEC relational object tr_file.
 *7 If text_file is NULL or non-valid (eg "-"), then stdin is used.
 *7 If da_file is NULL, then error.
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
#include "dir_array.h"
#include "textline.h"


static long mode;

static SPEC_PARAM spec_args[] = {
    {"text_da_obj.rwmode", getspec_filemode, (char *) &mode},
    TRACE_PARAM ("text_da_obj.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


int
init_text_da_obj (spec, unused)
SPEC *spec;
char *unused;
{
    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_text_da_obj");
    PRINT_TRACE (2, print_string, "Trace: leaving init_text_da_obj");

    return (0);
}

int
text_da_obj (text_file, da_file, inst)
char *text_file;
char *da_file;
int inst;
{
    DIR_ARRAY da;

    FILE *in_fd;
    int da_fd;
    char line_buf[PATH_LEN];
    SM_TEXTLINE textline;
    char *token_array[64];
    float *float_array;
    float *begin_float_array;
    long size_float_array;
    float *endmark_float_array;  /* set to */
                                 /* &begin_float_array[size_float_array-64] */
    long id_num;
    long i;
    long num_tokens = 0;

    PRINT_TRACE (2, print_string, "Trace: entering text_da_obj");

    if (VALID_FILE (text_file)) {
        if (NULL == (in_fd = fopen (text_file, "r")))
            return (UNDEF);
    }
    else
        in_fd = stdin;

    if (da_file == (char *) NULL) {
        set_error (SM_ILLPA_ERR, "Illegal dir_array_file", "text_da_obj");
        return (UNDEF);
    }

    if (UNDEF == (da_fd = open_dir_array (da_file, mode))) {
        clr_err();
        if (UNDEF == (da_fd = open_dir_array (da_file, mode | SCREATE)))
            return (UNDEF);
    }

    textline.max_num_tokens = 64;
    textline.tokens = token_array;

    size_float_array = 1024;
    if (NULL == (begin_float_array = (float *)
                 malloc ((unsigned) size_float_array * sizeof (float))))
        return (UNDEF);
    endmark_float_array = &begin_float_array[size_float_array-64];
    float_array = begin_float_array;

    da.id_num = -1;
    da.num_list = 0;
    da.list = (char *) begin_float_array;
    
    while (NULL != fgets (line_buf, PATH_LEN, in_fd)) {
        /* Break line_buf up into tokens */
        (void) text_textline (line_buf, &textline);
        id_num = atol (textline.tokens[0]);
        if (id_num != da.id_num) {
            /* Output previous vector, if exists */
            if (da.id_num != -1) {
                da.num_list *= sizeof (float);
                if (UNDEF == seek_dir_array (da_fd, &da) ||
                    UNDEF == write_dir_array (da_fd, &da))
                    return (UNDEF);
            }
            else
                num_tokens = textline.num_tokens;
            da.id_num = id_num;
            da.num_list = 0;
            float_array = begin_float_array;
        }
        
        if (textline.num_tokens != num_tokens) {
            set_error (SM_ILLPA_ERR, "Illegal input line", "text_da_obj");
            return (UNDEF);
        }
        if (endmark_float_array <= float_array) {
            size_float_array += size_float_array;
            if (NULL == (begin_float_array = (float *)
                         realloc ((char *) begin_float_array,
                               (unsigned) size_float_array * sizeof (float))))
                return (UNDEF);
            endmark_float_array = &begin_float_array[size_float_array-64];
            float_array = &begin_float_array[da.num_list];
            da.list = (char *) begin_float_array;
        }
        for (i = 1; i < num_tokens; i++)
            *float_array++ = atof (token_array[i]);
        da.num_list += num_tokens - 1;
    }  

    /* Output previous vector */
    if (da.num_list > 0) {
        da.num_list *= sizeof (float);
        if (UNDEF == seek_dir_array (da_fd, &da) ||
            UNDEF == write_dir_array (da_fd, &da))
            return (UNDEF);
    }

    if (UNDEF == close_dir_array (da_fd)) {
        return (UNDEF);
    }

    if (VALID_FILE (text_file))
        (void) fclose (in_fd);

    PRINT_TRACE (2, print_string, "Trace: leaving text_da_obj");
    return (1);
}

int
close_text_da_obj (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_text_da_obj");
    PRINT_TRACE (2, print_string, "Trace: leaving close_text_da_obj");
    
    return (0);
}
