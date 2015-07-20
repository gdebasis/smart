#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/parse_full.c,v 11.2 1993/02/03 16:50:58 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Parse a standard text section, returning concepts and location info
 *1 index.parse_sect.full
 *2 full_parse (token, consect, inst)
 *3   SM_TOKENSECT *token;
 *3   SM_CONSECT *consect;
 *3   int inst;
 *4 init_full_parse(spec, unused)
 *5   "index.parse_sect.trace"
 *5   "index.parse.single_case"
 *5   "index.parse.section.<token->section_num>.{word,proper,number}.token_to_con"
 *5   "index.parse.section.<token->section_num>.{word,proper,number}.ctype"
 *4 close_full_parse(inst)
 *7 Take the tokens given by token and produce a set of corresponding concepts
 *7 Normalize token to be all lower case, call appropriate token_to_con 
 *7 routine which is responsible for stemming, stopwords, and conversion to
 *7 concept. Assign appropriate ctype.
 *7 Keep track of number of word, sentence, and paragraph seen.
 *8 * Input of section parser, giving the tokens found in the text of a 
 *8 doc, along with the tokentype *
 *8 typedef struct {
 *8     char *token;
 *8     long tokentype;
 *8 } SM_TOKENTYPE;                     * Individual token *
 *8 
 *8 typedef struct {
 *8     long section_num;               * Index of the section_id for section *
 *8     long num_tokens;
 *8     SM_TOKENTYPE *tokens;
 *8 } SM_TOKENSECT;                     * Tokens for a section *
 *8 
 *8 * Output of section parser *
 *8 typedef struct {
 *8     long con;                   * Concept number of input token *
 *8     long ctype;                 * Ctype of con *
 *8     long sect_num;              * Section within document *
 *8     long para_num;              * Paragraph within section *
 *8     long sent_num;              * Sentence number within section *
 *8     long word_num;             * Token number within section *
 *8 } SM_CONLOC;
 *8 
 *8 typedef struct {
 *8     long num_concepts;
 *8     SM_CONLOC *concepts;
 *8 } SM_CONSECT;

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

static int single_case_flag;     /* set if collection is all one case,
                                    so case cannot be used when checking
                                    for sentence boundaries */

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("index.parse_sect.trace")
    {"index.parse.single_case",    getspec_bool,  (char *) &single_case_flag},
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

/* Each of word, proper, and number (the three parsetypes recognized by
   parse_full), will have a PARSETYPE_FUNC structure defined for it, which
   gives the operations to be done on it (gotten either directly from the
   spec file, or indirectly through the standard doc_desc structure).*/
typedef struct {
    long ctype;
    PROC_INST token_to_con;
} PARSETYPE_FUNC;

#define SM_PT_F_WORD  0
#define SM_PT_F_PROPER  1
#define SM_PT_F_NUMBER  2

#define SM_PF_NUM_PF 3

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    SM_CONLOC *conloc;        
    unsigned max_num_conloc;
    long num_conloc;

    PARSETYPE_FUNC parse_func[SM_PF_NUM_PF];
    int single_case;

    long para_num, sent_num;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static int init_parsetype_func(), add_con();


int 
init_full_parse(spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    STATIC_INFO *ip;
    int new_inst;
    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_full_parse");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];


    /* For each of word, proper, number find the parsetype_func associated
       with that type of token, and initialize that function */
    if (UNDEF == init_parsetype_func ("word",
                                       spec,
                                       param_prefix,
                                       &ip->parse_func[SM_PT_F_WORD]) ||
        UNDEF == init_parsetype_func ("proper",
                                       spec,
                                       param_prefix,
                                       &ip->parse_func[SM_PT_F_PROPER]) ||
        UNDEF == init_parsetype_func ("number",
                                       spec,
                                       param_prefix,
                                       &ip->parse_func[SM_PT_F_NUMBER]))
        return (UNDEF);

    ip->max_num_conloc = 0;
    ip->single_case = single_case_flag;

    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_full_parse");

    return (new_inst);
}

int
full_parse (tokensect, consect, inst)
SM_TOKENSECT *tokensect;          /* Input tokens */
SM_CONSECT *consect;
int inst;
{
    STATIC_INFO *ip;
    SM_TOKENTYPE *token;
    char *token_ptr;
    int first_capital, other_capital;
    int last_word_ended_sent = 1;
    long i;

    PRINT_TRACE (2, print_string, "Trace: entering full_parse");
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
        ip->max_num_conloc += tokensect->num_tokens;
        if (NULL == (ip->conloc = (SM_CONLOC *)
                     malloc (ip->max_num_conloc * sizeof (SM_CONLOC))))
            return (UNDEF);
    }

    /* Initialize conloc info */
    ip->para_num = ip->sent_num = 0;
    ip->num_conloc = 0;

    for (i = 0; i < tokensect->num_tokens; i++) {
        token = &tokensect->tokens[i];
        switch (token->tokentype) {
          case SM_INDEX_LOWER:
            if (UNDEF == add_con (ip,
                                  token->token,
                                  &ip->parse_func[SM_PT_F_WORD],
                                  i))
                return (UNDEF);
            last_word_ended_sent = 0;
            break;
          case SM_INDEX_MIXED:
            token_ptr = token->token;
            first_capital = 0;
            other_capital = 0;
            if (! islower ((unsigned char) *token_ptr)) {
                if (isupper ((unsigned char) *token_ptr))
                    *token_ptr = tolower ((unsigned char) *token_ptr);
                first_capital = 1;
                token_ptr++;
            }
            while (*token_ptr) {
                if (! islower ((unsigned char) *token_ptr)) {
                    if (isupper ((unsigned char) *token_ptr))
                        *token_ptr = tolower ((unsigned char) *token_ptr);
                    other_capital = 1;
                }
                token_ptr++;
            }
            /* Check to see if capitalized because start of sentence */
            /* or because all single case (therefore no proper nouns) */
            if (ip->single_case ||
                (first_capital && other_capital == 0 &&
                 last_word_ended_sent &&
                 (token->token[1] != '\0' ||
                  token->token[0] == 'a' ||
                  token->token[0] == 'i'))) {
                /* More than one letter (or A or I), only first
                   capitalized, at beginning of sentence */
                if (UNDEF == add_con (ip,
                                      token->token,
                                      &ip->parse_func[SM_PT_F_WORD],
                                      i))
                    return (UNDEF);
            }
            else {
                if (UNDEF == add_con (ip,
                                      token->token,
                                      &ip->parse_func[SM_PT_F_PROPER],
                                      i))
                    return (UNDEF);
            }
            last_word_ended_sent = 0;
            break;
          case SM_INDEX_DIGIT:
            if (UNDEF == add_con (ip,
                                  token->token,
                                  &ip->parse_func[SM_PT_F_NUMBER],
                                  i))
                return (UNDEF);
            last_word_ended_sent = 0;
            break;
          case SM_INDEX_SPACE:
            break;
          case SM_INDEX_NLSPACE:
            if (check_paragraph (token, last_word_ended_sent)) {
		if (!last_word_ended_sent) {
		    ip->sent_num++;
		    last_word_ended_sent = 1;
		}
                ip->para_num++;
            }
            break;
          case SM_INDEX_SENT:
            last_word_ended_sent = 1;
            ip->sent_num++;
            break;
          case SM_INDEX_PARA:
            ip->para_num++;
	    if (!last_word_ended_sent) {
		last_word_ended_sent = 1;
		ip->sent_num++;
	    }
            break;
	    
	    /* If this is a sentence break and we had seen some non-
	     * concept token (punctuation), then make sure that the
	     * sent/para they occurred in is known. If this punc ends
	     * a sent, then another sent end will have to be counted
	     * if it occurs (as in "...") so we fake that we've seen
	     * a non-concept token */
          case SM_INDEX_PUNC:
            if (check_sentence (token, i, tokensect->tokens + tokensect->num_tokens, 
				ip->single_case)) {
                last_word_ended_sent = 1;
                ip->sent_num++;
            }
	    else
		last_word_ended_sent = 0;	/* since it didn't */
            break;

          case SM_INDEX_IGNORE:
            break;

	  case SM_INDEX_SECT_END:   /* occurs so check_sentence sees end */
	    break;

          case SM_INDEX_SECT_START:
          default:
            return (UNDEF);
        }
    }

    consect->concepts = ip->conloc;
    consect->num_concepts = ip->num_conloc;
    consect->num_tokens_in_section = tokensect->num_tokens;
    
    PRINT_TRACE (4, print_consect, consect);
    PRINT_TRACE (2, print_string, "Trace: leaving full_parse");

    return (1);
}

int 
close_full_parse(inst)
int inst;
{
    STATIC_INFO *ip;
    long i;

    PRINT_TRACE (2, print_string, "Trace: entering close_full_parse");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_full_parse");
        return (UNDEF);
    }

    ip  = &info[inst];
    ip->valid_info--;

    if (ip->valid_info == 0) {
        if (0 != ip->max_num_conloc)
            (void) free ((char *) ip->conloc);
        for (i = 0; i < SM_PF_NUM_PF; i++) {
            if (UNDEF == ip->parse_func[i].ctype)
                continue;
            if (UNDEF == ip->parse_func[i].token_to_con.ptab->close_proc
                (ip->parse_func[i].token_to_con.inst))
                return (UNDEF);
        }
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_full_parse");

    return (0);
}


static int
init_parsetype_func (parsetype, spec, prefix, parse_func)
char *parsetype;
SPEC *spec;
char *prefix;
PARSETYPE_FUNC *parse_func;
{
    char buf[PATH_LEN];
    char *ptr = buf;
    char *end_buf;
    char *old_ptr = prefix;
    SPEC_PARAM spec_generic;

    while (*old_ptr)
        *ptr++ = *old_ptr++;
    old_ptr = parsetype;
    while (*old_ptr)
        *ptr++ = *old_ptr++;
    *ptr++ = '.';
    end_buf = ptr;

    spec_generic.param = buf;

    (void) strcpy (end_buf, "ctype");
    spec_generic.result = (char *) &parse_func->ctype;
    spec_generic.convert = getspec_long;
    if (UNDEF == lookup_spec (spec, &spec_generic, 1))
        return (UNDEF);
    if (parse_func->ctype == UNDEF)
        /* This type not indexed */
        return (0);

    (void) strcpy (end_buf, "token_to_con");
    spec_generic.result = (char *) &parse_func->token_to_con.ptab;
    spec_generic.convert = getspec_func;
    if (UNDEF == lookup_spec (spec, &spec_generic, 1))
        return (UNDEF);
    *end_buf = '\0';
    if (UNDEF == (parse_func->token_to_con.inst =
                  parse_func->token_to_con.ptab->init_proc (spec, buf)))
        return (UNDEF);

    return (1);
}


/* Add token to the conloc info if it fits criteria (token_to_con 
   assigns a non-UNDEF concept number) */
static int
add_con (ip, token, parse_func, token_num)
STATIC_INFO *ip;
char *token;
PARSETYPE_FUNC *parse_func;
long token_num;
{
    long con;
    SM_CONLOC *conloc;

    if (parse_func->ctype == UNDEF)
        return (0);

    /* Convert to a concept number */
    if (UNDEF == parse_func->token_to_con.ptab->proc
                 (token, &con, parse_func->token_to_con.inst))
        return (UNDEF);

    if (UNDEF == con)
        return (0);

    conloc = &ip->conloc[ip->num_conloc];
    conloc->con = con;
    conloc->ctype = parse_func->ctype;
    conloc->para_num = ip->para_num;
    conloc->sent_num = ip->sent_num;
    conloc->token_num = token_num;

    ip->num_conloc++;
    return (1);
}
