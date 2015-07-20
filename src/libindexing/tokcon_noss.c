#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/tokcon_noss.c,v 11.2 1993/02/03 16:51:17 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Get con corresponding to token from a dictionary. No stemming or stopwords.
 *1 index.token_to_con.dict_noss
 *2 tokcon_dict_noss (word, con, inst)
 *3   char *word;
 *3   long *con;
 *3   int inst;

 *4 init_tokcon_dict_noss (spec, param_prefix)
 *5   "index.token_to_con.trace"
 *5   "*.ctype"
 *5   "index.parse.ctype.*.dict_file"
 *5   "index.parse.ctype.*.dict_file.rmode"
 *5   "index.parse.ctype.*.dict_file.rwmode"
 *5   "index.parse.ctype.*.dict_file_size"
 *4 close_tokcon_dict_noss (inst)

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
#include "dict.h"
#include "io.h"
#include "docindex.h"
#include "trace.h"
#include "context.h"
#include "inst.h"
#include "rel_header.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("index.token_to_con.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static char *prefix;
static long ctype;
static SPEC_PARAM_PREFIX pspec_args[] = {
    {&prefix,  "ctype",           getspec_long,    (char *) &ctype},
    };
static int num_pspec_args = sizeof (pspec_args) / sizeof (pspec_args[0]);

static char *prefix_ctype;
static char *dict_file;
static long dict_rmode;
static long dict_rwmode;
static long dict_file_size;
static SPEC_PARAM_PREFIX p2spec_args[] = {
    {&prefix_ctype,  "dict_file",     getspec_dbfile,    (char *) &dict_file},
    {&prefix_ctype,  "dict_file.rmode", getspec_filemode,(char *) &dict_rmode},
    {&prefix_ctype,  "dict_file.rwmode",getspec_filemode,(char *) &dict_rwmode},
    {&prefix_ctype,  "dict_file_size", getspec_long,   (char *) &dict_file_size},
    };
static int num_p2spec_args = sizeof (p2spec_args) / sizeof (p2spec_args[0]);


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

int
init_tokcon_dict_noss (spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    STATIC_INFO *ip;
    long i;
    int new_inst;
    REL_HEADER rh;
    char prefix_buf[48];
    long dict_mode;

    /* Lookup the values of the relevant global parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    prefix = param_prefix;
    if (UNDEF == lookup_spec_prefix (spec, &pspec_args[0], num_pspec_args))
        return (UNDEF);

    (void) sprintf (prefix_buf, "index.parse.ctype.%ld.", ctype);
    prefix_ctype = prefix_buf;
    if (UNDEF == lookup_spec_prefix (spec, &p2spec_args[0], num_p2spec_args))
        return (UNDEF);
    
    PRINT_TRACE (2, print_string, "Trace: entering init_tokcon_dict_noss");

    /* If this dict_file has already been opened for dict con, just use
       that instantiation */
    for (i = 0; i < max_inst; i++) {
        if (info[i].valid_info && 0 == strcmp (dict_file, info[i].file_name)) {
            info[i].valid_info++;
            PRINT_TRACE (2, print_string, "Trace: leaving init_tokcon_dict_noss");
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
        dict_mode = dict_rwmode;
    else
        dict_mode = dict_rmode;
    /* Open dictionary, creating if needed */
    if (UNDEF == (ip->fd = open_dict (dict_file, dict_mode))) {
        if (! check_context (CTXT_INDEXING_DOC)) 
            return (UNDEF);
        /* dict_file doesn't exist.  Must create */
        clr_err();
        /* Create file with given size */
	rh.type = S_RH_DICT;
        rh.max_primary_value = dict_file_size;
        if (UNDEF == create_dict (dict_file, &rh))
            return (UNDEF);
        /* Open newly created dict_file */
        if (UNDEF == (ip->fd = open_dict (dict_file, dict_mode)))
            return (UNDEF);
    }

    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_tokcon_dict_noss");

    return (new_inst);
}

/* Lookup word in the dictionary, return the concept corresponding to it,
   entering it if necessary.
*/
int
tokcon_dict_noss (word, con, inst)
char *word;                     /* token to be entered */
long *con;
int inst;
{
    DICT_ENTRY dict;
    int status;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering tokcon_dict_noss");
    PRINT_TRACE (6, print_string, word);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "tokcon_dict_noss");
        return (UNDEF);
    }
    ip  = &info[inst];

    dict.con = UNDEF;
    dict.info = 0;
    dict.token = word;
    if (UNDEF == (status = seek_dict(ip->fd, &dict))) {
        return (UNDEF);
    }
    if (status == 0) {
        /* word is not in dictionary. Enter it if indexing doc. */
        /* Otherwise, dict.con remains UNDEF */
        if (ip->doc_flag) {
            /* When write_dict returns, dict will have correct con entered */
            if (1 != (status = write_dict (ip->fd, &dict)))
                return (UNDEF);
        }
    }
    else {
        /* word is already in dictionary, just get concept */
        if (1 != read_dict (ip->fd, &dict))
            return (UNDEF);
    }
    *con = dict.con;
    
    PRINT_TRACE (4, print_long, con);
    PRINT_TRACE (2, print_string, "Trace: leaving tokcon_dict_noss");

    return (status);
}

int
close_tokcon_dict_noss (inst)
int inst;
{
    STATIC_INFO *ip;
    PRINT_TRACE (2, print_string, "Trace: entering close_tokcon_dict_noss");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_tokcon_dict_noss");
        return (UNDEF);
    }
    ip  = &info[inst];
    ip->valid_info--;

    if (0 == ip->valid_info) {
        if (UNDEF == close_dict (ip->fd))
            return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_tokcon_dict_noss");

    return (0);
}


