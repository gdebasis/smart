#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/text_dict_o.c,v 11.2 1993/02/03 16:49:32 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Take an entire text file, and add its tokens to dictionary
 *1 convert.obj.text_dict_obj
 *2 text_dict_obj (text_file, dict_file, inst)
 *3 char *text_file;
 *3 char *dict_file;
 *3 int inst;
 *4 init_text_dict_obj (spec, dict_size_ptr)
 *5   "text_dict_obj.dict_file_size"
 *5   "text_dict_obj.dict_file"
 *5   "text_dict_obj.dict_file.rwmode"
 *5   "convert.text_dict_obj.trace"
 *4 close_text_dict_obj (inst)
 *7 Read text dictionary entries from text_file, convert to DICT_ENTRY
 *7 tuples (via procedure text_dict), and write to the dictionary dict_file.
 *7 Dictionary is created with size *dict_size_ptr if specified, else with
 *7 value of parameter dict_file_size.  If text_file is NULL or non-valid
 *7 (eg "-"), then stdin is used.  If dict_file is NULL, then value of
 *7 dict_file spec parameter is used.
 *7 Return UNDEF if error, else 1;
***********************************************************************/

#include <fcntl.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "dict.h"
#include "buf.h"

static long dict_size;
static long dict_mode;
static char *default_dict_file;

/* Convert the given text_entry to a dictionary form */
static SPEC_PARAM spec_args[] = {
    {"text_dict_obj.dict_file_size",getspec_long,   (char *) &dict_size},
    {"text_dict_obj.dict_file",     getspec_dbfile, (char *) &default_dict_file},
    {"text_dict_obj.dict_file.rwmode",getspec_filemode,(char *) &dict_mode},
    TRACE_PARAM ("convert.text_dict_obj.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

int text_dict ();

/* Lookup the default dictionary and dict_size.  If the second arg is non-NULL
   then it gives a new dict_size to use (this is used, for example, to create
   stopword files) */
int
init_text_dict_obj (spec, dict_size_ptr)
SPEC *spec;
long *dict_size_ptr;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    if (NULL != dict_size_ptr)
        dict_size = *dict_size_ptr;

    PRINT_TRACE (2,print_string, "Trace: entering/leaving init_text_dict_obj");
    return (0);
}

int
text_dict_obj (text_file, dict_file, inst)
char *text_file;
char *dict_file;
int inst;
{
    FILE *text_fd;
    int dict_fd;
    DICT_ENTRY dict;
    REL_HEADER rh;
    SM_BUF sm_buf;
    char buf[MAX_TOKEN_LEN];
    int status;

    PRINT_TRACE (2, print_string, "Trace: entering text_dict_obj");
    
    if (VALID_FILE (text_file)) {
        if (NULL == (text_fd = fopen (text_file, "r")))
            return (UNDEF);
    }
    else
        text_fd = stdin;

    if (dict_file == (char *) NULL)
        dict_file = default_dict_file;

    if (UNDEF == (dict_fd = open_dict (dict_file, dict_mode))) {
        clr_err();
        rh.max_primary_value = dict_size;
	rh.type = S_RH_DICT;
        if (UNDEF == create_dict (dict_file, &rh))
            return (UNDEF);
        if (UNDEF == (dict_fd = open_dict (dict_file, dict_mode)))
            return (UNDEF);
    }

    sm_buf.buf = &buf[0];
    sm_buf.size = MAX_TOKEN_LEN;
    while (NULL != fgets (buf, MAX_TOKEN_LEN, text_fd)) {
        sm_buf.end = strlen (buf) - 1;
        if (UNDEF == (status = text_dict (&sm_buf, &dict, 0)))
            return (UNDEF);
        if (status) {
            if (UNDEF == seek_dict (dict_fd, &dict) ||
                UNDEF == write_dict (dict_fd, &dict))
                return (UNDEF);
        }
    }

    if (UNDEF == close_dict (dict_fd))
        return (UNDEF);

    if (VALID_FILE (text_file)) 
        (void) fclose (text_fd);

    PRINT_TRACE (2, print_string, "Trace: leaving text_dict_obj");
    return (1);
}

int
close_text_dict_obj (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering/leaving close_text_dict_obj");
    return (0);
}
