#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libindexing/tokcon_niss.c,v 11.2 1993/02/03 16:52:06 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Get con corresponding to token from a dictionary. No stemming or stopwords.
 *1 local.index.token_to_con.dict_niss
 *2 tokcon_dict_niss (word, con, inst)
 *3   char *word;
 *3   long *con;
 *3   int inst;

 *4 init_tokcon_dict_niss (spec, param_prefix)
 *5   "index.token_to_con.trace"
 *5   "*.ctype"
 *5   "ctype.*.dict_file"
 *5   "ctype.*.dict_file.rmode"
 *5   "ctype.*.dict_file.rwmode"
 *5   "ctype.*.dict_file_size"
 *4 close_tokcon_dict_niss (inst)

 *6 global_context is checked to see if indexing doc (CTXT_INDEXING_DOC).

 *7 Lookup the ctype specified by parameter given by the concatenation
 *7 of param_prefix and "ctype".  Normally, param_prefix will have some
 *7 value like "section.1.word.". The dictionary file to use is then
 *7 looked up using the parameter name "ctype.<ctype>.dict_file".
 *7 
 *7 If token is not in dictionary, and CTXT_INDEXING_DOC is set, then enter
 *7 token in dictionary and set con.  If CTXT_INDEXING_DOC is not set, 
 *7 then con will get value UNDEF.
 *7 If CTXT_INDEXING_DOC is set and the dictionary does not exist, then it
 *7 is created with initial size dict_file_size.
 *7 
 *7 No stemming, stopwords, or dictionary initailization is done.
 *7 UNDEF returned if error, 1 if token is in dictionary, 0 if not.
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
#include "rel_header.h"

int open_dict_noinfo(), create_dict_noinfo(), seek_dict_noinfo(), 
    write_dict_noinfo(), read_dict_noinfo(), close_dict_noinfo();

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("index.token_to_con.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;
    /* procedure dependant info */
    int doc_flag;             /* True if CTXT_INDEXING_DOC */
    int fd;
    char file_name[PATH_LEN];
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static int save_spec_id = 0;
    
int
init_tokcon_dict_niss (spec, param_prefix)
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
    long dict_file_size;
    REL_HEADER rh;
    long ctype;

    /* Lookup the values of the relevant global parameters */
    if (spec->spec_id != save_spec_id &&
        UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    /* Lookup the filename for this instantiation */
    if (param_prefix != NULL) 
        (void) strcpy (param, param_prefix);
    else
        param[0] = '\0';
    (void) strcat (param, "ctype");
    dict_param.param = param;
    dict_param.convert = getspec_long;
    dict_param.result = (char *) &ctype;
    if (UNDEF == lookup_spec (spec, &dict_param, 1))
        return (UNDEF);
    
    /* Lookup the filename for this instantiation */
    (void) sprintf (param, "ctype.%ld.dict_file", ctype);
    dict_param.param = param;
    dict_param.convert = getspec_dbfile;
    dict_param.result = (char *) &dict_file;
    if (UNDEF == lookup_spec (spec, &dict_param, 1))
        return (UNDEF);
    
    PRINT_TRACE (2, print_string, "Trace: entering init_tokcon_dict_niss");

    /* If this dict_file has already been opened for dict con, just use
       that instantiation */
    for (i = 0; i < max_inst; i++) {
        if (info[i].valid_info && 0 == strcmp (dict_file, info[i].file_name)) {
            info[i].valid_info++;
            PRINT_TRACE (2, print_string, "Trace: leaving init_tokcon_dict_niss");
            return (i);
        }
    }

    /* Use new instantiation */
    NEW_INST (new_inst);
    if (new_inst == UNDEF)
        return (UNDEF);

    ip = &info[new_inst];
            
    (void) strncpy (ip->file_name, dict_file, PATH_LEN);

    ip->doc_flag = check_context (CTXT_INDEXING_DOC) ? 1 : 0;

    if (! VALID_FILE (dict_file))
        return (UNDEF);

    /* Get dict_mode */
    if (ip->doc_flag)
        (void) sprintf (param, "ctype.%ld.dict_file.rwmode", ctype);
    else
        (void) sprintf (param, "ctype.%ld.dict_file.rmode", ctype);
    dict_param.param = param;
    dict_param.convert = getspec_filemode;
    dict_param.result = (char *) &dict_mode;
    if (UNDEF == lookup_spec (spec, &dict_param, 1))
        return (UNDEF);

    /* Open dictionary, creating if needed */
    if (UNDEF == (ip->fd = open_dict_noinfo (dict_file, dict_mode))) {
        if (! check_context (CTXT_INDEXING_DOC)) 
            return (UNDEF);
        /* dict_file doesn't exist.  Must create */
        clr_err();
        /* Get dict_size */
        (void) sprintf (param, "ctype.%ld.dict_file_size", ctype);
        dict_param.param = param;
        dict_param.convert = getspec_long;
        dict_param.result = (char *) &dict_file_size;
        if (UNDEF == lookup_spec (spec, &dict_param, 1))
            return (UNDEF);
        /* Create file with given size */
        rh.max_primary_value = dict_file_size;
        if (UNDEF == create_dict_noinfo (dict_file, &rh))
            return (UNDEF);
        /* Open newly created dict_file */
        if (UNDEF == (ip->fd = open_dict_noinfo (dict_file, dict_mode)))
            return (UNDEF);
    }

    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_tokcon_dict_niss");

    return (new_inst);
}

/* Lookup word in the dictionary, return the concept corresponding to it,
   entering it if necessary.
*/
int
tokcon_dict_niss (word, con, inst)
char *word;                     /* token to be entered */
long *con;
int inst;
{
    DICT_NOINFO dict;
    int status;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering tokcon_dict_niss");
    PRINT_TRACE (6, print_string, word);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "tokcon_dict_niss");
        return (UNDEF);
    }
    ip  = &info[inst];

    dict.con = UNDEF;
    dict.token = word;
    if (UNDEF == (status = seek_dict_noinfo(ip->fd, &dict))) {
        return (UNDEF);
    }
    if (status == 0) {
        /* word is not in dictionary. Enter it if indexing doc. */
        /* Otherwise, dict.con remains UNDEF */
        if (ip->doc_flag) {
            /* When write_dict_noinfo returns, dict will have correct con entered */
            if (1 != (status = write_dict_noinfo (ip->fd, &dict)))
                return (UNDEF);
        }
    }
    else {
        /* word is already in dictionary, just get concept */
        if (1 != read_dict_noinfo (ip->fd, &dict))
            return (UNDEF);
    }
    *con = dict.con;
    
    PRINT_TRACE (4, print_long, con);
    PRINT_TRACE (2, print_string, "Trace: leaving tokcon_dict_niss");

    return (status);
}

int
close_tokcon_dict_niss (inst)
int inst;
{
    STATIC_INFO *ip;
    PRINT_TRACE (2, print_string, "Trace: entering close_tokcon_dict_niss");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_tokcon_dict_niss");
        return (UNDEF);
    }
    ip  = &info[inst];
    ip->valid_info--;

    if (0 == ip->valid_info) {
        if (UNDEF == close_dict_noinfo (ip->fd))
            return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_tokcon_dict_niss");

    return (0);
}



