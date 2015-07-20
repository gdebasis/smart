#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/stop_dict.c,v 11.2 1993/02/03 16:51:00 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Determine if word is not to be indexed because in stop dictionary
 *1 index.stop.stop_dict
 *2 stop_dict (word, stopword_flag, inst)
 *3   char *word;
 *3   int *stopword_flag;
 *3   int inst;

 *4 init_stop_dict (spec, param_prefix)
 *5    "index.stop.trace"
 *5    "*.stop_file"
 *5    "*.text_stop_file"
 *5    "*.stop_file_size"
 *4 close_stop_dict (inst)

 *7 Lookup word in the dictionary given by stop_file. If in dict, then
 *7 stopword_flag set to 1, else 0.
 *7 Return UNDEF if error, 0 otherwise.
 *7 Stop_file is a section-map dependant parameter (different stopword
 *7 dictionaries can be used for different types of info occuring in
 *7 different sections). The stopfile to
 *7 be used for this instantiation of stop_dict is given by looking up
 *7 strcat (param_prefix, "stop_file") in the spec file.  Normally,
 *7 param_prefix will be something like "index.section.2.proper" to indicate
 *7 this stop file is to be used for proper nouns in section 2.
 *7
 *7 If the dictionary version of stop_file does not exist yet, it is created
 *7 from the English text version in text_stop_file; with a dict size
 *7 indicated by stop_file_size.

 *9 Bug: should use .rmode and .rwcmode as modes to open/create stopword file
 *9 Really should use tries for this instead of a dictionary.

***********************************************************************/
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "dict.h"
#include "io.h"
#include "docindex.h"
#include "trace.h"
#include "inst.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("index.stop.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static char *prefix;
static char *stop_file;
static char *text_stop_file;
static long stop_file_size;
static SPEC_PARAM_PREFIX prefix_spec_args[] = {
    {&prefix,   "stop_file",       getspec_dbfile,   (char *) &stop_file},
    {&prefix,   "text_stop_file",  getspec_string,   (char *) &text_stop_file},
    {&prefix,   "stop_file_size",  getspec_long,     (char *) &stop_file_size},
    };
static int num_prefix_spec_args = sizeof (prefix_spec_args) / sizeof (prefix_spec_args[0]);


/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    int fd;                  /* file descriptor for dictionary */
    char file_name[PATH_LEN];
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;


int
init_stop_dict (spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    long i;
    STATIC_INFO *ip;
    int new_inst;
    int temp_inst;

    /* Lookup the values of the relevant global parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);
    prefix = param_prefix;
    if (UNDEF == lookup_spec_prefix (spec,
                                     &prefix_spec_args[0],
                                     num_prefix_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_stop_dict");
    PRINT_TRACE (6, print_string, stop_file);

    /* Check to see if this file_name has already been initialized.  If
       so, that instantiation will be used. */
    for (i = 0; i < max_inst; i++) {
        if (info[i].valid_info && 0 == strcmp (stop_file,
                                               info[i].file_name)) {
            info[i].valid_info++;
            PRINT_TRACE (2, print_string, "Trace: leaving init_stop_dict");
            return (i);
        }
    }

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];

    (void) strncpy (ip->file_name, stop_file, PATH_LEN);
    if (UNDEF == (ip->fd = open_dict (stop_file, (long) SRDONLY|SINCORE))) {
        /* stop_file doesn't exist.  Must create */
        clr_err();
        if (UNDEF == (temp_inst = init_text_dict_obj (spec, &stop_file_size))||
            UNDEF == text_dict_obj (text_stop_file, stop_file, temp_inst) ||
            UNDEF == close_text_dict_obj (temp_inst))
            return (UNDEF);
        /* Open newly created stop_file */
        if (UNDEF == (ip->fd = open_dict (stop_file, (long) SRDONLY|SINCORE)))
            return (UNDEF);
    }

    ip->valid_info = 1;

    return (new_inst);
}

/* Return 1 in stopword_flag if word is in stopword dictionary, 0 otherwise */
int
stop_dict (word, stopword_flag, inst)
char *word;                     /* word to be checked if stopword */
long *stopword_flag;
int inst;
{
    DICT_ENTRY dict;

    PRINT_TRACE (2, print_string, "Trace: entering stop_dict");
    PRINT_TRACE (6, print_string, word);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "stop_dict");
        return (UNDEF);
    }

    dict.con = UNDEF;
    dict.info = 0;
    dict.token = word;
    if (UNDEF == (*stopword_flag = seek_dict(info[inst].fd, &dict))) {
        return (UNDEF);
    }
    
    PRINT_TRACE (4, print_long, stopword_flag);
    PRINT_TRACE (2, print_string, "Trace: leaving stop_dict");

    return (0);
}

int
close_stop_dict (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_stop_dict");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_stop_dict");
        return (UNDEF);
    }

    ip  = &info[inst];
    ip->valid_info--;
    if (ip->valid_info == 0) {
        if (UNDEF == close_dict (ip->fd))
            return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_stop_dict");

    return (0);
}
