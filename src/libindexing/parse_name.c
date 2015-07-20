#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/parse_name.c,v 11.2 1993/02/03 16:50:59 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Parse tokenized line as a name, and get standardized name
 *1 index.parse_sect.name
 *2 name_parse (token, consect, inst)
 *3   SM_TOKENSECT *token;
 *3   SM_CONSECT *consect;
 *3   int inst;
 *4 init_name_parse(spec, unused)
 *5   "index.parse.section.<token->section_num>.word.ctype"
 *5   "index.parse.section.<token->section_num>.word.token_to_con"
 *5   "index.parse_sect.trace"
 *4 close_name_parse(inst)
 *7 Parse a name: each line is assumed to be one name; it cannot
 *7 start with a ".".  Each name is put into a standard form of    
 *7 "lastname, fi"  if the first initial is given. If not, just the 
 *7 (assumed to be) surname is used.  The name is assigned the appropriate
 *7 ctype, and token_to_con is called to get the concept number.
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

/* name_parse:
        Return a name: each line is assumed to be one name; it cannot
           start with a ".".  Each name is put into a standard form of    
           "LastName, F"  if the first initial is given. If not, just the 
           (assumed to be) surname is returned.                           
*/



static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("index.parse_sect.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


#define NP_MAX_PATTERN 6

#define NP_NAME 1
#define NP_COMMA 2
#define NP_SUFFIX 3
#define NP_HYPHEN 4


static char *get_name(), *get_buf();
static void addbuf(), char_addbuf();
static int pattern_match();
static int add_con();
#ifdef TRACE
static void print_name();
#endif  /* TRACE */

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    SM_CONLOC *conloc;        
    unsigned max_num_conloc;
    long num_conloc;

    long ctype;
    PROC_INST token_to_con;
    long token_num, sent_num;       /* location info */
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;


int 
init_name_parse(spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    STATIC_INFO *ip;
    int new_inst;
    SPEC_PARAM spec_generic;
    char buf[PATH_LEN];

    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_name_parse");
    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];

    spec_generic.param = buf;

    (void) strcpy (buf, param_prefix);
    (void) strcat (buf, "word.ctype");
    spec_generic.result = (char *) &ip->ctype;
    spec_generic.convert = getspec_long;
    if (UNDEF == lookup_spec (spec, &spec_generic, 1))
        return (UNDEF);
    if (ip->ctype != UNDEF) {
        /* Determine and initialize the token_to_con procedure */
        (void) strcpy (buf, param_prefix);
        (void) strcat (buf, "word.token_to_con");
        spec_generic.result = (char *) &ip->token_to_con.ptab;
        spec_generic.convert = getspec_func;
        if (UNDEF == lookup_spec (spec, &spec_generic, 1))
            return (UNDEF);
        buf[strlen(buf) - strlen ("token_to_con")] = '\0';
        if (UNDEF == (ip->token_to_con.inst =
                      ip->token_to_con.ptab->init_proc (spec, buf)))
            return (UNDEF);
    }

    ip->max_num_conloc = 0;
    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_name_parse");

    return (new_inst);
}

static char *suffixes[] = {
    "II",
    "III",
    "IV",
    "Jr",
};

static int num_suffixes = sizeof (suffixes) / sizeof (suffixes[0]);

typedef struct {
    char *token;
    int type;
} NAME_TOKEN_TYPE;



/* Find names.  First, decide which input tokens compose the next name,
   Then check in patterns to decide how to normalize name - ie decide
   which of the input tokens are first name and last name.
   Finally output normalized name
*/

int
name_parse (tokensect, consect, inst)
SM_TOKENSECT *tokensect;          /* Input tokens */
SM_CONSECT *consect;
int inst;
{
    STATIC_INFO *ip;
    long i,j;
    SM_TOKENTYPE *token;

    int num_tokens = 0;
    NAME_TOKEN_TYPE name[NP_MAX_PATTERN];           

    char *final_name = NULL;        /* Name after initialization */

    PRINT_TRACE (2, print_string, "Trace: entering name_parse");
    PRINT_TRACE (6, print_tokensect, tokensect);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "full_parse");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Reserve space for conlocs.  In full_parse (as opposed to some
       phrasing methods) we are guaranteed to have less conlocs than
       tokens */
    if (tokensect->num_tokens > ip->max_num_conloc) {
        if (ip->max_num_conloc > 0)
            (void) free ((char *) ip->conloc);
        ip->max_num_conloc = tokensect->num_tokens;
        if (NULL == (ip->conloc = (SM_CONLOC *)
                     malloc (ip->max_num_conloc * sizeof (SM_CONLOC))))
            return (UNDEF);
    }

    ip->num_conloc = 0;
    ip->token_num = 0;
    ip->sent_num = 0;

    /* Initialize the token_type array for name */
    for (j = 0; j < NP_MAX_PATTERN; j++) {
        name[j].type = 0;
    }

    for (i = 0; i < tokensect->num_tokens; i++) {
        token = &tokensect->tokens[i];
        switch (token->tokentype) {
          case SM_INDEX_LOWER:
          case SM_INDEX_MIXED:
            if (num_tokens < NP_MAX_PATTERN) {
                name[num_tokens].token = token->token;
                for (j = 0;
                     j < num_suffixes && strcmp(suffixes[j],
                                                name[num_tokens].token);
                     j++)
                    ;
                if (j < num_suffixes) {
                    name[num_tokens].type  = NP_SUFFIX;
                }
                else {
                    name[num_tokens].type  = NP_NAME;
                }
                num_tokens++;
            }
            break;
          case SM_INDEX_PUNC:
            if (*token->token == ',') {
                name[num_tokens].type  = NP_COMMA;
                num_tokens++;
            }
            else if (*token->token == '-') {
                name[num_tokens].type  = NP_HYPHEN;
                num_tokens++;
            }
            break;
          case SM_INDEX_NLSPACE:
            if (num_tokens > 0) {
                final_name = get_name(name, num_tokens);
#ifdef TRACE    
                if (global_trace && sm_trace > 3) {
                    print_name (name, num_tokens, final_name);
                    (void) fflush (stdout);
                }
#endif /* TRACE */
                if (final_name != NULL && 
                    UNDEF == add_con (ip, final_name))
                    return (UNDEF);
                num_tokens = 0;
                /* Initialize the token_type array for name */
                for (j = 0; j < NP_MAX_PATTERN; j++) {
                    name[j].type = 0;
                }
            }
            break;
          case SM_INDEX_SPACE:
          case SM_INDEX_SECT_START:
          case SM_INDEX_SECT_END:
          case SM_INDEX_IGNORE:
          case SM_INDEX_DIGIT:
          case SM_INDEX_SENT:
          case SM_INDEX_PARA:
          default:
            break;
        }
        ip->token_num++;
    }

    consect->concepts = ip->conloc;
    consect->num_concepts = ip->num_conloc;
    consect->num_tokens_in_section = tokensect->num_tokens;

    PRINT_TRACE (4, print_consect, consect);
    PRINT_TRACE (2, print_string, "Trace: leaving name_parse");

    return (1);
}

int 
close_name_parse(inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_name_parse");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_name_parse");
        return (UNDEF);
    }

    ip  = &info[inst];
    ip->valid_info--;

    if (ip->valid_info == 0) {
        if (0 != ip->max_num_conloc)
            (void) free ((char *) ip->conloc);
        if (UNDEF != ip->ctype &&
            UNDEF == ip->token_to_con.ptab->close_proc (ip->token_to_con.inst))
            return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_name_parse");
    return (0);
}


static struct {
    int num_types;
    int types[NP_MAX_PATTERN];
    int last;
    int last_part2;
    int first;
} patterns[] = {
    {1,   {1, 0, 0, 0, 0, 0},   0, -1, -1},    /* Last  only */
    {3,   {1, 2, 1, 0, 0, 0},   0, -1, 2},     /* Last  "," First  */
    {4,   {1, 2, 1, 1, 0, 0},   0, -1, 2},     /* Last  "," First Middle  */
    {5,   {1, 2, 1, 1, 3, 0},   0, -1, 2},     /* Last  "," First Middle  Suffix */
    {4,   {1, 2, 1, 3, 0, 0},   0, -1, 2},     /* Last "," First Suffix */
    {2,   {1, 1, 0, 0, 0, 0},   1, -1, 0},     /* First  Last  */
    {3,   {1, 1, 3, 0, 0, 0},   1, -1, 0},     /* First  Last  Suffix*/
    {4,   {1, 1, 2, 3, 0, 0},   1, -1, 0},     /* First  Last  "," Suffix*/
    {4,   {1, 1, 2, 1, 0, 0},   0,  1, 3},     /* Last Last2 "," First */
    {5,   {1, 1, 2, 1, 1, 0},   0,  1, 3},     /* Last Last2 "," First Middle */
    {6,   {1, 1, 2, 1, 1, 1},   0,  1, 3},     /* Last Last2 "," First Middle Mid*/
    {3,   {1, 1, 1, 0, 0, 0},   2, -1, 0},     /* First  Middle Last  */
    {4,   {1, 1, 1, 1, 0, 0},   3, -1, 0},     /* First  Middle Middle Last  */
    {5,   {1, 1, 1, 2, 3, 0},   2, -1, 0},     /* First  Middle Last "," Suffix */
    {5,   {1, 1, 1, 2, 1, 0},   2,  0, 4},     /* Last Last  Last "," First */
    {3,   {1, 4, 1, 0, 0, 0},   0,  2, -1},    /* Last  only */
    {5,   {1, 4, 1, 2, 1, 0},   0,  2, 4},     /* Last  "," First  */
    {6,   {1, 4, 1, 2, 1, 1},   0,  2, 4},     /* Last  "," First  Middle */
    {4,   {1, 1, 4, 1, 0, 0},   3,  1, 0},     /* First  Last  */
};

static int num_patterns = sizeof (patterns) / sizeof (patterns[0]);

static
char *get_name (name, num_tokens)
NAME_TOKEN_TYPE *name;
int num_tokens;
{
    long i;
    int last_name_flag;

    /* Clear accumulated name buffer */
    addbuf ((char *) NULL);
    /* Find pattern of name */
    for (i = 0; i < num_patterns; i++) {
        if (pattern_match (name, num_tokens, i)) {
            addbuf (name[patterns[i].last].token);
            /* Add second part of first name (eg hyphenated name, "van ..." */
            if (patterns[i].last_part2 >= 0) {
                addbuf ("-");
                addbuf (name[patterns[i].last_part2].token);
            }
            /* Add first initial if it exists */
            if (patterns[i].first >= 0) {
                addbuf (", ");
                char_addbuf (name[patterns[i].first].token);
            }
            break;
        }
    }
    if (i >= num_patterns) {
        /* Just use first available token as last name, second as first name */
        last_name_flag = 1;
        for (i = 0; i < num_tokens; i++) {
            if (name[i].type == NP_NAME) {
                if (last_name_flag) {
                    addbuf (name[i].token);
                    last_name_flag = 0;
                }
                else {
                    addbuf (", ");
                    char_addbuf (name[i].token);
                    break;
                }
            }
        }
    }
    return (get_buf());
}

static int
pattern_match (name, num_tokens, ind)
NAME_TOKEN_TYPE *name;
int num_tokens;
long ind;
{
    long i;

    if (patterns[ind].num_types != num_tokens)
        return (0);
    for (i = 0; i < num_tokens; i++) {
        if (patterns[ind].types[i] != name[i].type)
            return (0);
    }
    return (1);
}


static char buf[MAX_TOKEN_LEN];
static char *buf_ptr;

static void
addbuf (string)
char *string;
{
    if (string == NULL) {
        buf_ptr = &buf[0];
    }
    else {
        while (*string && buf_ptr < &buf[MAX_TOKEN_LEN-1]) {
            if (isupper ((unsigned char) *string))
                *buf_ptr = tolower ((unsigned char) *string);
            else
                *buf_ptr = *string;
            buf_ptr++; string++;
        }
    }
    *buf_ptr = '\0';
}

/* Add the first char of string to buf */
static void
char_addbuf (string)
char *string;
{
    if (string == NULL) {
        buf_ptr = &buf[0];
    }
    else {
        if (buf_ptr < &buf[MAX_TOKEN_LEN-1]) {
            if (isupper ((unsigned char) *string))
                *buf_ptr = tolower ((unsigned char) *string);
            else
                *buf_ptr = *string;
            buf_ptr++;
        }
    }
    *buf_ptr = '\0';
}

static char *
get_buf ()
{
    if (buf[0] != '\0') {
        return (&buf[0]);
    }
    return (NULL);
}


#ifdef TRACE
static void
print_name (name, num_tokens, final_name)
NAME_TOKEN_TYPE *name;
int num_tokens;
char *final_name;
{
    long i;
    for (i = 0; i < num_tokens; i++) {
        switch (name[i].type) {
          case NP_NAME:
          case NP_SUFFIX:
            (void) fprintf (global_trace_fd, "%s ", name[i].token);
            break;
          case NP_COMMA:
            (void) fprintf (global_trace_fd, ", ");
            break;
          case NP_HYPHEN:
            (void) fprintf (global_trace_fd, "- ");
            break;
          default:
            (void) fprintf (global_trace_fd, "ERROR ILLEGAL TYPE");
        }
    }
    (void) fprintf (global_trace_fd, "\n");
    if (final_name != NULL)
        (void) fprintf (global_trace_fd, "  %s\n", final_name);
}
#endif  /* TRACE */

/* Add token to the conloc info if it fits criteria.  Here, check to see
   if stopword, then stem, and then call token_to_con to get concept number */
static int
add_con (ip, token)
STATIC_INFO *ip;
char *token;
{

    SM_CONLOC *conloc = &ip->conloc[ip->num_conloc];
    if (ip->ctype == UNDEF)
        return (0);

    /* Convert to a concept number */
    if (UNDEF == ip->token_to_con.ptab->proc
                 (token, &conloc->con, ip->token_to_con.inst))
        return (UNDEF);

    if (UNDEF == conloc->con)
        return (0);

    conloc->ctype = ip->ctype;
    conloc->para_num = 0;
    conloc->sent_num = ip->sent_num++;
    conloc->token_num = ip->token_num;

    ip->num_conloc++;
    return (1);
}
