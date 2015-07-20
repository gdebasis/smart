#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/tokcon_dict.c,v 11.2 1993/02/03 16:51:17 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Convert a parse token to a concept using a dictionary.
 *1 index.token_to_con.dict
 *2 tokcon_dict (token, consect, inst)
 *3   SM_TOKENSECT *token;
 *3   SM_CONSECT *consect;
 *3   int inst;
 *4 init_tokcon_dict(spec, unused)
 *5   "index.token_to_con.trace"
 *5   "index.parse.section.*.*.ctype"
 *5   "index.parse.section.*.*.stem_wanted"
 *5   "index.parse.section.*.*.stopword_wanted"
 *5   "index.parse.section.*.*.thesaurus_wanted"
 *5   "index.parse.ctype.*.dict_file"
 *5   "index.parse.ctype.*.dict_file.rmode"
 *5   "index.parse.ctype.*.dict_file.rwmode"
 *5   "index.parse.ctype.*.dict_file_size"
 *5   "index.parse.ctype.*.stemmer"
 *5   "index.parse.ctype.*.text_stop_file"
 *5   "index.parse.ctype.*.text_thesaurus_file"
 *4 close_tokcon_dict(inst)
 *7 Take the token and produce the corresponding concept by looking up
 *7 token in dict_file.
 *7 If concept is a stopword then con is set to UNDEF and 0 is returned.
 *7 If the concept is not in the dictionary and we are not adding to 
 *7 dictionary (CTXT_INDEXING_DOC is not set), then con is set 
 *7 to UNDEF and 0 is returned.  
 *7 If the concept is in the dictionary or we are adding to the dictionary,
 *7 then con is set to concept corresponding to token's stem and 1 is returned.

 *8 Implementation is to have a single dictionary (dict_file) which uses
 *8 the info field of a token's entry to indicate the final concept.
 *8 
 *8 The unstemmed token is looked up in the dictionary. If it exists, then
 *8 the info field is checked.  If info is UNDEF, then token is a stopword.
 *8 If info is positive, then that value indicates the concept to be 
 *8 assigned to token.  If the value is 0, then the final concept hasn't been
 *8 assigned yet.  The token is stemmed, the stem is entered in the
 *8 dictionary, and the info field of the original token is set to the
 *8 concept corresponding to the stem.  The same procedure is followed
 *8 if the original token did not exist at all in the dictionary.
 *8 Note that if the context is not indexing a document (CTXT_INDEXING_DOC)
 *8 then no changes are made to the dictionary and UNDEF is returned as
 *8 the concept (so users' query terms will not fill up the dictionary!)
 *8 
 *8 Upon procedure initialization, if the dictionary doesn't exist, it
 *8 must be created and initialized.  Each word (one word per line, 
 *8 interior blanks are significant) in
 *8 text_stop_file is added to the dictionary with an info field of UNDEF
 *8 (assuming that text_stop_file is a valid file).  
 *8 
 *8 Then, if thesaurus_file exists and is valid, thesaurus entries are
 *8 added the dictionary.  Each line of thesaurus file has a pair of
 *8 tokens (separated by tabs) indicating token and token_class.
 *8 Token_class is added to the dictionary (with an info field of 0), and
 *8 then token is added with an info field of the concept number assigned
 *8 to token_class.  Note that this means you need a line of 'token_class
 *8 token_class' if you wish to ensure that token_class is a member of
 *8 itself.  Also note that there is no stemming involved here.

 *9 In this implementation, the stemming and stopword procedures are
 *9 ctype dependent instead of parse_type (eg section.1.word) dependent.
 *9 By the nature of the procedure, there can be only one stem associated
 *9 with a token in the dictionary.  Stemming can be turned on and off
 *9 based on parse_type though, (eg, stem normal words but don't stem
 *9 proper nouns).  Behavior of the procedure is not well defined if
 *9 a single dictionary is being used for multiple ctypes and those
 *9 ctypes use different stemmers or stopwords.  Behavior may also be
 *9 undefined if stemming but not stopwords are turned on for a parsetype
 *9 and a stopword from another parsetype is received.

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
static int  stopword_wanted;
static int  thesaurus_wanted;
static PROC_TAB *stemmer;
static SPEC_PARAM_PREFIX pspec_args[] = {
    {&prefix,  "ctype",           getspec_long,    (char *) &ctype},
    {&prefix,  "stem_wanted",  getspec_bool,    (char *) &stem_wanted},
    {&prefix,  "stopword_wanted", getspec_bool,    (char *) &stopword_wanted},
    {&prefix,  "thesaurus_wanted",getspec_bool,    (char *) &thesaurus_wanted},
    {&prefix,  "stemmer",        getspec_func,     (char *) &stemmer},
    };
static int num_pspec_args = sizeof (pspec_args) / sizeof (pspec_args[0]);

static char *prefix_ctype;
static char *dict_file;
static long dict_rmode;
static long dict_rwmode;
static char *stopword_file;
static char *thesaurus_file;
static long dict_file_size;
static SPEC_PARAM_PREFIX p2spec_args[] = {
    {&prefix_ctype,  "dict_file",     getspec_dbfile,    (char *) &dict_file},
    {&prefix_ctype,  "dict_file.rmode", getspec_filemode,(char *) &dict_rmode},
    {&prefix_ctype,  "dict_file.rwmode",getspec_filemode,(char *) &dict_rwmode},
    {&prefix_ctype,  "text_stop_file", getspec_string,  (char *) &stopword_file},
    {&prefix_ctype,  "text_thesaurus_file",getspec_string,
                                       (char *) &thesaurus_file},
    {&prefix_ctype,  "dict_file_size", getspec_long,   (char *) &dict_file_size},
    };
static int num_p2spec_args = sizeof (p2spec_args) / sizeof (p2spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    long stem_wanted;
    long stopword_wanted;
    long thesaurus_wanted;

    int indexing_doc;
    char dict_file[PATH_LEN];
    int dict_fd;
    int num_dict_shared;   /* Number of instantiations that share this
                              dictionary.  (Only one dict_fd can
                              be opened for writing, so instantiations
                              must share that fd) */
    PROC_INST stemmer;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static int init_dict(), add_stem();


int 
init_tokcon_dict(spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    STATIC_INFO *ip;
    int new_inst;
    long dict_mode;
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
    
    PRINT_TRACE (2, print_string, "Trace: entering init_tokcon_dict");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];

    if (check_context (CTXT_INDEXING_DOC)) {
        ip->indexing_doc = 1;
        dict_mode = dict_rwmode;
    }
    else {
        ip->indexing_doc = 0;
        dict_mode = dict_rmode;
    }

    ip->stem_wanted = stem_wanted;
    ip->stopword_wanted = stopword_wanted;
    ip->thesaurus_wanted = thesaurus_wanted;
    ip->stemmer.ptab = stemmer;
    (void) strcpy (ip->dict_file, dict_file);

    if (ip->stem_wanted) {
	if (UNDEF == (ip->stemmer.inst = stemmer->init_proc (spec, prefix)))
	    return (UNDEF);
    }

    /* Check to see if dictionary has been opened by another instantiation.
       If so, will share that dict_fd.  If not, will open
       dict_file, initializing it if it needs to be created */
    ip->num_dict_shared = 1;
    for (i = 0; i < max_inst; i++) {
        if (i == new_inst)
            continue;
        if (info[i].valid_info > 0 &&
            0 == strcmp (ip->dict_file, info[i].dict_file)) {
            ip->num_dict_shared = ++info[i].num_dict_shared;
            ip->dict_fd = info[i].dict_fd;
        }
    }

    if (ip->num_dict_shared == 1) {
        /* Not sharing this dict_file, must open and initialize on own */
        if (UNDEF == (ip->dict_fd = open_dict (dict_file, dict_mode))){
            /* dict_file doesn't exist.  Must create and initialize */
            clr_err();
            if (UNDEF == init_dict () ||
                UNDEF == (ip->dict_fd = open_dict (dict_file, dict_mode)))
                return (UNDEF);
        }
    }

    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_tokcon_dict");

    return (new_inst);
}

int
tokcon_dict (token, con, inst)
char *token;
long *con;
int inst;
{
    STATIC_INFO *ip;
    int status;
    DICT_ENTRY dict;

    PRINT_TRACE (2, print_string, "Trace: entering tokcon_dict");
    PRINT_TRACE (6, print_string, token);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "tokcon_dict");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (token == (char *) NULL) {
        *con = UNDEF;
        PRINT_TRACE (2, print_string, "Trace: leaving tokcon_dict");
        return (0);
    }

    /* Look up token in dictionary */
    dict.token = token;
    dict.con = UNDEF;
    if (UNDEF == (status = (seek_dict (ip->dict_fd, &dict))))
        return (UNDEF);
    if (0 == status) {
        /* Token not in dictionary.  Add if indexing_doc, otherwise 
           check to see if stem is in dict else return 0 */

        /* Stem token, if desired */
        if (ip->stem_wanted) {
            if (UNDEF == (dict.info = add_stem (ip, token)))
                return (UNDEF);
            
            if (! ip->indexing_doc) {
                if (dict.info == 0) {
                    *con = UNDEF;
                    PRINT_TRACE (2, print_string,"Trace: leaving tokcon_dict");
                    return (0);
                }
            }
            else {
                /* Note that add_stem may change location within dict so have
                   to seek_dict again */
                if (UNDEF == seek_dict (ip->dict_fd, &dict) ||
                    1 != write_dict (ip->dict_fd, &dict))
                    return (UNDEF);
            }
            *con = dict.info;
        }
        else {
            /* Stem not wanted. Add token if indexing doc else return 0 */
            if (! ip->indexing_doc) {
                *con = UNDEF;
                PRINT_TRACE (2, print_string, "Trace: leaving tokcon_dict");
                return (0);
            }
            dict.info = 0;
            if (1 != write_dict (ip->dict_fd, &dict))
                return (UNDEF);
            *con = dict.con;
        }
    }
    else {
        /* Token is in dictionary. */
        if (UNDEF == read_dict (ip->dict_fd, &dict))
            return (UNDEF);
        /* Is token a stopword? */
        if (ip->stopword_wanted && UNDEF == dict.info) {
            *con = UNDEF;
            PRINT_TRACE (2, print_string, "Trace: leaving tokcon_dict");
            return (0);
        }
        if ((ip->stem_wanted || ip->thesaurus_wanted) &&
            0 < dict.info) {
            /* Stem or thesaurus entry has been cached */
            *con = dict.info;
        }
        else if (ip->stem_wanted) {
            /* Token is in dictionary, but must look up stem since it
               has not been cached. */
            if (UNDEF == (*con = add_stem (ip, token)))
                return (UNDEF);
            if (*con == 0) {
                /* Stem is not in dictionary (and wasn't added, so we
                   are not indexing_doc). */
                *con = UNDEF;
                PRINT_TRACE (2, print_string, "Trace: leaving tokcon_dict");
                return (0);
            }
            if (0 == dict.info && ip->indexing_doc) {
                /* "Cache" the stem info */
                dict.info = *con;
                if (UNDEF == seek_dict (ip->dict_fd, &dict) ||
                    1 != write_dict (ip->dict_fd, &dict))
                    return (UNDEF);
            }
        }
        else {
            /* Stemming not wanted; just return the token's concept */
            *con = dict.con;
        }
    }

    PRINT_TRACE (4, print_long, con);
    PRINT_TRACE (2, print_string, "Trace: leaving tokcon_dict");

    return (1);
}

int 
close_tokcon_dict(inst)
int inst;
{
    STATIC_INFO *ip;
    long i;

    PRINT_TRACE (2, print_string, "Trace: entering close_tokcon_dict");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_tokcon_dict");
        return (UNDEF);
    }

    ip  = &info[inst];
    ip->valid_info--;

    if (ip->valid_info == 0) {
        /* Release dictionary if no longer shared */
        if (1 < ip->num_dict_shared) {
            for (i = 0; i < max_inst; i++) {
                if (i == inst)
                    continue;
                if (info[i].valid_info > 0 &&
                    0 == strcmp (ip->dict_file, info[i].dict_file)) {
                    --info[i].num_dict_shared;
                }
            }
        }
        else {
            if (UNDEF == close_dict (ip->dict_fd))
                return (UNDEF);
        }

        /* Close stemmer */
        if (ip->stem_wanted &&
            UNDEF == ip->stemmer.ptab->close_proc (ip->stemmer.inst))
            return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_tokcon_dict");

    return (0);
}


static int
add_stem (ip, token)
STATIC_INFO *ip;
char *token;
{
    char stemmed_token[MAX_TOKEN_LEN];
    int status;
    DICT_ENTRY stemmed_dict;

    (void) strcpy (stemmed_token, token);
    if (UNDEF == ip->stemmer.ptab->proc (stemmed_token,
                                         &stemmed_dict.token,
                                         ip->stemmer.inst))
        return (UNDEF);
    stemmed_dict.con = UNDEF;
    if (UNDEF == (status = seek_dict (ip->dict_fd, &stemmed_dict)))
        return (UNDEF);
    if (status == 0 && ! ip->indexing_doc) {
        /* stem not in dictionary, and not indexing doc, so we won't add it */
        return (0);
    }
    if (status == 0) {
        stemmed_dict.info = 0;
        if (1 != write_dict (ip->dict_fd, &stemmed_dict))
            return (UNDEF);
    }
    else {
        if (1 != read_dict (ip->dict_fd, &stemmed_dict))
            return (UNDEF);
    }
    return (stemmed_dict.con);
}

static int
init_dict ()
{
    char buf[PATH_LEN];
    DICT_ENTRY dict, class_dict;
    REL_HEADER rh;
    char *ptr;
    int status;
    int dict_fd;
    FILE *text_fd;

    /* Create the dictionary */
    rh.type = S_RH_DICT;
    rh.max_primary_value = dict_file_size;
    if (UNDEF == create_dict (dict_file, &rh))
        return (UNDEF);
    if (UNDEF == (dict_fd = open_dict (dict_file, dict_rwmode)))
        return (UNDEF);

    /* Must ensure that concept 0 is not a valid token (especially not
       a valid stem) */
    dict.con = 0;
    dict.token = NULL;
    if (UNDEF == seek_dict (dict_fd, &dict))
        return (UNDEF);
    dict.token = "NOT-a-TOKEN";
    dict.info = UNDEF;
    if (UNDEF == write_dict (dict_fd, &dict))
        return (UNDEF);

    /* Add stopwords to dictionary */
    if (VALID_FILE (stopword_file)) {
        if (NULL == (text_fd = fopen (stopword_file, "r")))
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
    }

    /* Add thesaurus entries to dictionary */
    if (VALID_FILE (thesaurus_file)) {
        if (NULL == (text_fd = fopen (thesaurus_file, "r")))
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
            while (*ptr && *ptr != '\t')
                ptr++;
            if (*ptr != '\t')
                /* Illformed line - Just ignore */
                continue;
            class_dict.token = ptr + 1;
            ptr--;
            while (*ptr == ' ')
                ptr--;
            *(ptr+1) = '\0';

            ptr = class_dict.token;
            while (*ptr && *ptr != '\n')
                ptr++;
            ptr--;
            while (*ptr == ' ')
                ptr--;
            *(ptr+1) = '\0';

            class_dict.con = UNDEF;
            class_dict.info = 0;
            if (UNDEF == (status = seek_dict (dict_fd, &class_dict)))
                return (UNDEF);
            if (status == 0) {
                if (UNDEF == write_dict (dict_fd, &class_dict))
                    return (UNDEF);
            }
            else {
                if (1 != read_dict (dict_fd, &class_dict))
                    return (UNDEF);
            }

            dict.con = UNDEF;
            dict.info = class_dict.con;
            if (UNDEF == seek_dict (dict_fd, &dict) ||
                UNDEF == write_dict (dict_fd, &dict))
                return (UNDEF);
        }
        (void) fclose (text_fd);
    }

    if (UNDEF == close_dict (dict_fd))
        return (UNDEF);
    
    return (1);
}
