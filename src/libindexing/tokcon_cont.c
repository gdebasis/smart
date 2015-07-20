#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/tokcon_cont.c,v 11.2 1993/02/03 16:51:15 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Convert a parse token to a concept using controlled vocabulary
 *1 index.token_to_con.dict_cont
 *2 tokcon_dict_cont (token, consect, inst)
 *3   SM_TOKENSECT *token;
 *3   SM_CONSECT *consect;
 *3   int inst;
 *4 init_tokcon_dict_cont(spec, unused)
 *5   "index.token_to_con.trace"
 *5   "index.parse.section.*.*.ctype"
 *5   "index.parse.ctype.*.dict_file"
 *5   "index.parse.ctype.*.text_controlled_file"
 *4 close_tokcon_dict_cont(inst)
 *7 Take the token and produce the corresponding concept by looking up
 *7 token in dict_file.
 *7 If the concept is not in the dictionary  then con is set 
 *7 to UNDEF and 0 is returned.  

 *8 Upon procedure initialization, if the dictionary doesn't exist, it
 *8 must be created and initialized.  Each word (one word per line, 
 *8 interior blanks are significant) in text_controlled_file is 
 *8 added to the dictionary.  It is an error if text_controlled_file
 *8 is not a valid file.

***********************************************************************/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "docindex.h"
#include "trace.h"
#include "inst.h"
#include "context.h"
#include "dict.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("index.token_to_con.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static char *prefix;
static long ctype;
static int  stem_wanted;
static PROC_TAB *stemmer;
static SPEC_PARAM_PREFIX pspec_args[] = {
    {&prefix,  "ctype",     getspec_long,    (char *) &ctype},
    {&prefix,  "stem_wanted",  getspec_bool,    (char *) &stem_wanted},
    {&prefix,  "stemmer",        getspec_func,     (char *) &stemmer},
    };
static int num_pspec_args = sizeof (pspec_args) / sizeof (pspec_args[0]);

static char *prefix_ctype;
static char *dict_file;
static long dict_rmode;
static long dict_rwmode;
static char *controlled_file;
static long dict_file_size;
static SPEC_PARAM_PREFIX p2spec_args[] = {
    {&prefix_ctype,  "dict_file",     getspec_dbfile,    (char *) &dict_file},
    {&prefix_ctype,  "dict_file.rmode", getspec_filemode,(char *) &dict_rmode},
    {&prefix_ctype,  "dict_file.rwmode",getspec_filemode,(char *) &dict_rwmode},
    {&prefix_ctype,  "text_controlled_file", getspec_string,
                                           (char *) &controlled_file},
    {&prefix_ctype,  "dict_file_size", getspec_long,   (char *) &dict_file_size},
    };
static int num_p2spec_args = sizeof (p2spec_args) / sizeof (p2spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    long stem_wanted;
    PROC_INST stemmer;

    char dict_file[PATH_LEN];
    int dict_fd;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static int init_dict();


int 
init_tokcon_dict_cont(spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    STATIC_INFO *ip;
    int new_inst;
    long i;
    char prefix_buf[48];

    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    prefix = param_prefix;
    if (UNDEF == lookup_spec_prefix (spec, &pspec_args[0], num_pspec_args))
        return (UNDEF);

    (void) sprintf (prefix_buf, "index.parse.ctype.%ld.", ctype);
    prefix_ctype = prefix_buf;
    if (UNDEF == lookup_spec_prefix (spec, &p2spec_args[0], num_p2spec_args))
        return (UNDEF);
    
    PRINT_TRACE (2, print_string, "Trace: entering init_tokcon_dict_cont");
    PRINT_TRACE (6, print_string, param_prefix);


    /* Check to see if dictionary has been opened by another instantiation.
       If so, will share that dict_fd.  If not, will open
       dict_file, initializing it if it needs to be created */
    for (i = 0; i < max_inst; i++) {
        if (info[i].valid_info > 0 &&
            0 == strcmp (dict_file, info[i].dict_file)) {
            info[i].valid_info++;
            return (i);
        }
    }

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];

    ip->stem_wanted = stem_wanted;
    ip->stemmer.ptab = stemmer;
    if (UNDEF == (ip->stemmer.inst = stemmer->init_proc (spec, prefix)))
        return (UNDEF);

    (void) strcpy (ip->dict_file, dict_file);

    /* Not sharing this dict_file, must open and initialize on own */
    if (UNDEF == (ip->dict_fd = open_dict (dict_file, dict_rmode))){
        /* dict_file doesn't exist.  Must create and initialize */
        clr_err();
        if (UNDEF == init_dict () ||
            UNDEF == (ip->dict_fd = open_dict (dict_file,
                                               dict_rmode)))
            return (UNDEF);
    }

    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_tokcon_dict_cont");

    return (new_inst);
}

int
tokcon_dict_cont (token, con, inst)
char *token;
long *con;
int inst;
{
    STATIC_INFO *ip;
    int status;
    DICT_ENTRY dict;
    char stemmed_token[MAX_TOKEN_LEN];

    PRINT_TRACE (2, print_string, "Trace: entering tokcon_dict_cont");
    PRINT_TRACE (6, print_string, token);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "tokcon_dict_cont");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (token == (char *) NULL) {
        *con = UNDEF;
        return (0);
    }

    /* Look up token in dictionary, possibly stemmed */
    dict.token = token;
    dict.con = UNDEF;
    if (ip->stem_wanted) {
	(void) strcpy (stemmed_token, token);
	if (UNDEF == ip->stemmer.ptab->proc (stemmed_token,
					     &dict.token,
					     ip->stemmer.inst))
	    return (UNDEF);
    }

    if (UNDEF == (status = (seek_dict (ip->dict_fd, &dict))))
        return (UNDEF);
    if (1 == status) {
        /* Token is in dictionary. */
        if (UNDEF == read_dict (ip->dict_fd, &dict))
            return (UNDEF);
        *con = dict.con;
    }
    else {
        *con = UNDEF;
    }

    PRINT_TRACE (4, print_long, con);
    PRINT_TRACE (2, print_string, "Trace: leaving tokcon_dict_cont");

    return (status);
}

int 
close_tokcon_dict_cont (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_tokcon_dict_cont");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_tokcon_dict_cont");
        return (UNDEF);
    }

    ip  = &info[inst];
    ip->valid_info--;

    if (ip->valid_info == 0) {
        if (UNDEF == close_dict (ip->dict_fd))
            return (UNDEF);
        /* Close stemmer */
        if (ip->stem_wanted &&
            UNDEF == ip->stemmer.ptab->close_proc (ip->stemmer.inst))
            return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_tokcon_dict_cont");

    return (0);
}

static int
init_dict ()
{
    DICT_ENTRY dict;
    REL_HEADER rh;
    char *ptr;
    int dict_fd;
    FILE *text_fd;
    char buf[MAX_TOKEN_LEN];

    /* Create the dictionary */
    rh.type = S_RH_DICT;
    rh.max_primary_value = dict_file_size;
    if (UNDEF == create_dict (dict_file, &rh))
        return (UNDEF);
    if (UNDEF == (dict_fd = open_dict (dict_file, dict_rwmode)))
        return (UNDEF);

    /* Add controlled vocabulary to dictionary */
    if (! VALID_FILE (controlled_file)) {
        set_error (SM_ILLPA_ERR, controlled_file, "tokcon_dict_cont");
        return (UNDEF);
    }

    if (NULL == (text_fd = fopen (controlled_file, "r")))
        return (UNDEF);
    while (NULL != fgets (buf, MAX_TOKEN_LEN, text_fd)) {
        /* "remove" leading and trailing blanks */
        ptr = buf;
        while (*ptr && *ptr == ' ')
            ptr++;
        if (! *ptr || *ptr == '\n')
            /* Illformed line - Just ignore */
            continue;
        dict.token = ptr;
        while (*ptr && *ptr != '\n')
            ptr++;
        ptr--;
        while (*ptr == ' ')
            ptr--;
        *(ptr+1) = '\0';
        
        dict.con = UNDEF;
        dict.info = UNDEF;
        if (UNDEF == seek_dict (dict_fd, &dict) ||
            UNDEF == write_dict (dict_fd, &dict))
            return (UNDEF);
    }
    (void) fclose (text_fd);
    if (UNDEF == close_dict (dict_fd))
        return (UNDEF);
    
    return (1);
}
