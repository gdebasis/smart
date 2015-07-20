#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libconvert/contok_di_ni.c,v 11.2 1993/02/03 16:51:41 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Get token corresponding to con from a dictionary (no info field).
 *1 local.convert.tup.contok_dict_noinfo
 *2 contok_dict_noinfo (con, word, inst)
 *3   long *con;
 *3   char **word;
 *3   int inst;

 *4 init_contok_dict_noinfo (spec, param_prefix)
 *5   ""index.contok.trace"
 *5   "*.dict_file"
 *5   "*.dict_file.rmode"
 *4 close_contok_dict_noinfo (inst)

 *7 Lookup con in the dictionary specified by parameter given by 
 *7 the concatenation of param_prefix and "dict_file".  
 *7 Normally, param_prefix will have some value like "ctype.1." in order
 *7 to obtain the ctype dependant dictionary.  Ie. "ctype.1.dict_file"
 *7 will be looked up in the specifications to find the correct dictionary
 *7 file to use.
 *7 If con is not in dictionary, word is set to "Not in Dictionary".
 *7 UNDEF returned if error, else 1 if word in dictionary, else 0
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "dict_noinfo.h"
#include "io.h"
#include "docindex.h"
#include "trace.h"
#include "context.h"
#include "inst.h"

int open_dict_noinfo(), create_dict_noinfo(), seek_dict_noinfo(), 
    read_dict_noinfo(), close_dict_noinfo();

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("index.contok.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;
    /* procedure dependant info */
    int fd;
    char file_name[PATH_LEN];
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static int save_spec_id = 0;
    
int
init_contok_dict_noinfo (spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    SPEC_PARAM dict_param;
    STATIC_INFO *ip;
    char param[PATH_LEN];
    long i;
    int new_inst;
    char *dict_file;
    long dict_mode;

    /* Lookup the values of the relevant global parameters */
    if (spec->spec_id != save_spec_id &&
        UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    /* Lookup the filename for this instantiation */
    if (param_prefix != NULL) 
        (void) strcpy (param, param_prefix);
    else
        param[0] = '\0';
    (void) strcat (param, "dict_file");
    dict_param.param = param;
    dict_param.convert = getspec_dbfile;
    dict_param.result = (char *) &dict_file;
    if (UNDEF == lookup_spec (spec, &dict_param, 1))
        return (UNDEF);
    
    PRINT_TRACE (2, print_string, "Trace: entering init_contok_dict_noinfo");

    /* If this dict_file has already been opened for dict con, just use
       that instantiation */
    for (i = 0; i < max_inst; i++) {
        if (info[i].valid_info && 0 == strcmp (dict_file, info[i].file_name)) {
            info[i].valid_info++;
            PRINT_TRACE (2, print_string, "Trace: leaving init_contok_dict_noinfo");
            return (i);
        }
    }

    /* Use new instantiation */
    NEW_INST (new_inst);
    if (new_inst == UNDEF)
        return (UNDEF);

    ip = &info[new_inst];
            
    (void) strncpy (ip->file_name, dict_file, PATH_LEN);

    if (! VALID_FILE (dict_file))
        return (UNDEF);

    /* Get dict_mode */
    if (param_prefix != NULL) 
        (void) strcpy (param, param_prefix);
    else
        param[0] = '\0';
    (void) strcat (param, "dict_file.rmode");
    dict_param.param = param;
    dict_param.convert = getspec_filemode;
    dict_param.result = (char *) &dict_mode;
    if (UNDEF == lookup_spec (spec, &dict_param, 1))
        return (UNDEF);

    /* Open dictionary */
    if (UNDEF == (ip->fd = open_dict_noinfo (dict_file, dict_mode))) {
        return (UNDEF);
    }

    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_contok_dict_noinfo");

    return (new_inst);
}

/* Lookup concept in the dictionary, return the token corresponding to it */
int
contok_dict_noinfo (con, word, inst)
long *con;
char **word;
int inst;
{
    DICT_NOINFO dict;
    int status;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering contok_dict_noinfo");
    PRINT_TRACE (6, print_long, con);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "contok_dict_noinfo");
        return (UNDEF);
    }
    ip  = &info[inst];

    dict.con = *con;
    dict.token = (char *) NULL;
    if (UNDEF == (status = seek_dict_noinfo (ip->fd, &dict))) {
        return (UNDEF);
    }
    if (status == 0) {
        /* word is not in dictionary.*/
        *word = "Not in Dictionary";
    }
    else {
        if (1 != (status = read_dict_noinfo (ip->fd, &dict)))
            *word = "Not in Dictionary";
        else
            *word = dict.token;
    }

    PRINT_TRACE (4, print_string, *word);
    PRINT_TRACE (2, print_string, "Trace: leaving contok_dict_noinfo");

    return (status);
}

int
close_contok_dict_noinfo (inst)
int inst;
{
    STATIC_INFO *ip;
    PRINT_TRACE (2, print_string, "Trace: entering close_contok_dict_noinfo");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_contok_dict_noinfo");
        return (UNDEF);
    }
    ip  = &info[inst];
    ip->valid_info--;

    if (0 == ip->valid_info) {
        if (UNDEF == close_dict_noinfo (ip->fd))
            return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_contok_dict_noinfo");

    return (0);
}



