#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/parse_adj.c,v 11.2 1993/02/03 16:50:53 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Parse a standard text section, returning concepts and adjacency phrases
 *1 local.index.parse_sect.chinese
 *2 chinese_parse (token, consect, inst)
 *3   SM_TOKENSECT *token;
 *3   SM_CONSECT *consect;
 *3   int inst;
 *4 init_chinese_parse(spec, unused)
 *5   "index.parse_sect.trace"
 *5   "index.parse.single_case"
 *5   "index.parse.section.<token->section_num>.{word,proper,number}.ctype"
 *5   "index.parse.section.<token->section_num>.{word,proper,number}.stemmer"
 *5   "index.parse.section.<token->section_num>.{word,proper,number}.stopword"
 *5   "index.parse.section.<token->section_num>.{word,proper,number}.token_to_con"
 *5   "index.parse.section.<token->section_num>.phrase.ctype"
 *5   "index.parse.phrase_distance"
 *4 close_chinese_parse(inst)
 *7 Take the tokens given by token and produce a set of corresponding concepts
 *7 Normalize token to be all lower case, call appropriate stopword checker,
 *7 appropriate stemmer, convert to concept, and assign appropriate ctype.
 *7 Keep track of number of word, sentence, and paragraph seen.
 *7 In addition, form two-term phrases by combining non-stopword stemmed
 *7 concepts which occur in the same sentence and are not separated by
 *7 more than phrase_distance non-whitespace tokens.  Phrase_distance of
 *7 0 indicates no phrasing, 1 indicates adjacency required, etc.  Phrase
 *7 components are ordered by alphabetic ordering.
 *7 Must use a token_to_con routine for the phrases that does not
 *7 do stemming or stopwords.
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
static long phrase_distance;

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("index.parse_sect.trace")
    {"index.parse.single_case",    getspec_bool,  (char *) &single_case_flag},
    {"index.parse.phrase_distance", getspec_long,  (char *) &phrase_distance},
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

/* Each of word, proper, number and phrase will have a PARSETYPE_FUNC
   structure defined for it, which gives the operations to be done 
   on it (gotten directly from the spec file) */
typedef struct {
    PROC_INST stemmer;
    PROC_INST stopword;
    long ctype;
    PROC_INST token_to_con;
} PARSETYPE_FUNC;

#define SM_PT_F_WORD  0
#define SM_PT_F_PROPER  1
#define SM_PT_F_NUMBER  2
#define SM_PT_F_PHRASE  3

#define SM_PF_NUM_PF 4


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

    /* Phrase info */
    long phrase_distance;
    long phrase_sentence;
    long num_phrase_comp;
    char **phrase_comp;         /* phrase_comp[0..phrase_distance] is a
                                   array of tokens possibly within phrase
                                   distance of the current token */
    long *comp_loc;             /* comp_loc[i] gives token_num of
                                   phrase_comp[i] */
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static int init_parsetype_func(), add_con(), add_comp();


int 
init_chinese_parse(spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    STATIC_INFO *ip;
    int new_inst;
    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_chinese_parse");

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
                                       &ip->parse_func[SM_PT_F_NUMBER]) ||
        UNDEF == init_parsetype_func ("phrase",
                                      spec,
                                      param_prefix,
                                      &ip->parse_func[SM_PT_F_PHRASE]))
        return (UNDEF);
    
    ip->max_num_conloc = 0;
    ip->single_case = single_case_flag;
    ip->phrase_distance = phrase_distance;

    if (NULL == (ip->phrase_comp = (char **)
                 malloc ((unsigned) phrase_distance * sizeof (char *))) ||
        NULL == (ip->comp_loc = (long *)
                 malloc ((unsigned) phrase_distance * sizeof (long))))
        return (UNDEF);

    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_chinese_parse");

    return (new_inst);
}

/* WARNING: for efficiency, chinese_parse changes the input token IN PLACE */

int
chinese_parse (tokensect, consect, inst)
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

    PRINT_TRACE (2, print_string, "Trace: entering chinese_parse");
    PRINT_TRACE (6, print_tokensect, tokensect);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "chinese_parse");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Reserve space for conlocs.  Note, in chinese_parse (as opposed to 
       non-phrase methods) we are not guaranteed to have less conlocs than
       tokens */
    if (2 * tokensect->num_tokens > ip->max_num_conloc) {
        if (ip->max_num_conloc > 0)
            (void) free ((char *) ip->conloc);
        ip->max_num_conloc = 2 * tokensect->num_tokens;
        if (NULL == (ip->conloc = (SM_CONLOC *)
                     malloc (ip->max_num_conloc * sizeof (SM_CONLOC))))
            return (UNDEF);
    }

    /* Initialize conloc info */
    ip->para_num = ip->sent_num = 0;
    ip->num_conloc = 0;
    ip->num_phrase_comp = 0;
    ip->phrase_sentence = 0;

    for (i = 0; i < tokensect->num_tokens; i++) {
        token = &tokensect->tokens[i];
        switch (token->tokentype) {
          case SM_INDEX_LOWER:
            if (UNDEF == add_con (ip,
                                  token->token,
                                  &ip->parse_func[SM_PT_F_WORD],
                                  i,
                                  0))
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
                                      i,
                                      0))
                    return (UNDEF);
            }
            else {
                if (UNDEF == add_con (ip,
                                      token->token,
                                      &ip->parse_func[SM_PT_F_PROPER],
                                      i,
                                      0))
                    return (UNDEF);
            }
            last_word_ended_sent = 0;
            break;
          case SM_INDEX_DIGIT:
            if (UNDEF == add_con (ip,
                                  token->token,
                                  &ip->parse_func[SM_PT_F_NUMBER],
                                  i,
                                  0))
                return (UNDEF);
            last_word_ended_sent = 0;
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
          case SM_INDEX_PUNC:
            if (check_sentence (token, ip->single_case)) {
                last_word_ended_sent = 1;
                ip->sent_num++;
            }
            break;
          case SM_INDEX_NLSPACE:
            if (check_paragraph (token, last_word_ended_sent)) {
                ip->para_num++;
		if (!last_word_ended_sent) {
		    last_word_ended_sent = 1;
		    ip->sent_num++;
		}
            }
            break;
          case SM_INDEX_SPACE:
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
    PRINT_TRACE (2, print_string, "Trace: leaving chinese_parse");

    return (1);
}

int 
close_chinese_parse(inst)
int inst;
{
    STATIC_INFO *ip;
    long i;

    PRINT_TRACE (2, print_string, "Trace: entering close_parse_sect");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_parse_sect");
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
            if (UNDEF == ip->parse_func[i].stemmer.ptab->close_proc
                (ip->parse_func[i].stemmer.inst) ||
                UNDEF == ip->parse_func[i].stopword.ptab->close_proc
                (ip->parse_func[i].stopword.inst) ||
                UNDEF == ip->parse_func[i].token_to_con.ptab->close_proc
                (ip->parse_func[i].token_to_con.inst))
                return (UNDEF);
        }
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_parse_sect");

    return (0);
}


/* This version of init_parsetype_func obtains all values from scratch (as
   opposed to obtaining from doc_desc).  It illustrates how to obtain these
   values in case there is some parsetype info desired that is not included
   in doc_desc. */
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

    (void) strcpy (end_buf, "stemmer");
    spec_generic.result = (char *) &parse_func->stemmer.ptab;
    spec_generic.convert = getspec_func;
    if (UNDEF == lookup_spec (spec, &spec_generic, 1))
        return (UNDEF);
    *end_buf = '\0';
    if (UNDEF == (parse_func->stemmer.inst = 
                  parse_func->stemmer.ptab->init_proc (spec, buf)))
        return (UNDEF);

    (void) strcpy (end_buf, "stopword");
    spec_generic.result = (char *) &parse_func->stopword.ptab;
    spec_generic.convert = getspec_func;
    if (UNDEF == lookup_spec (spec, &spec_generic, 1))
        return (UNDEF);
    *end_buf = '\0';
    if (UNDEF == (parse_func->stopword.inst =
                  parse_func->stopword.ptab->init_proc (spec, buf)))
        return (UNDEF);

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

/* Add token to the conloc info if it fits criteria.  Here, check to see
   if stopword, then stem, and then call token_to_con to get concept number */
static int
add_con (ip, token, parse_func, token_num, rec_call)
STATIC_INFO *ip;
char *token;
PARSETYPE_FUNC *parse_func;
long token_num;
int rec_call;                /* If 1, this is a recursive call in order to
                                add phrases */
{
    char phrase_buf[256];
    long token_range;
    long i;
    SM_CONLOC *conloc;

    PRINT_TRACE (8, print_string, "Trace: entering parse_adj.add_con");
    PRINT_TRACE (8, print_string, token);

    if (parse_func->ctype == UNDEF)
        return (0);

    /* Must check space to see if need more, since could be any number of 
       phrases being added */
    if (ip->num_conloc >= ip->max_num_conloc) {
        ip->max_num_conloc += ip->num_conloc;
        if (NULL == (ip->conloc = (SM_CONLOC *)
                     realloc ((char *) ip->conloc,
                              ip->max_num_conloc * sizeof (SM_CONLOC))))
            return (UNDEF);
    }

    conloc = &ip->conloc[ip->num_conloc];

    /* Convert to a concept number */
    if (UNDEF == parse_func->token_to_con.ptab->proc
                 (token, &conloc->con, parse_func->token_to_con.inst))
        return (UNDEF);

    if (UNDEF == conloc->con)
        return (0);

    conloc->ctype = parse_func->ctype;
    conloc->para_num = ip->para_num;
    conloc->sent_num = ip->sent_num;
    conloc->token_num = token_num;

    ip->num_conloc++;

    /* Add phrases, if not already doing that */
    if (rec_call)
        return (1);

    PRINT_TRACE (20, print_string, "Checking phrases");
    PRINT_TRACE (20, print_long, &ip->sent_num);
    PRINT_TRACE (20, print_long, &ip->phrase_sentence);

    if (ip->sent_num != ip->phrase_sentence) {
        /* Start of a new sentence */
        ip->phrase_sentence = ip->sent_num;
        ip->num_phrase_comp = 0;
        if (UNDEF == add_comp (ip, token, token_num))
            return (UNDEF);
        return (1);
    }

    token_range = token_num - phrase_distance;
    PRINT_TRACE (20, print_string, "Token_range");
    PRINT_TRACE (20, print_long, &token_range);
    for (i = 0; i < ip->num_phrase_comp; i++) {
        PRINT_TRACE (20, print_string, "Look at phrase");
        PRINT_TRACE (20, print_long, &i);
        PRINT_TRACE (20, print_long, &ip->comp_loc[i]);
        if (ip->comp_loc[i] >= token_range) {
            PRINT_TRACE (20, print_string, "Phrase found at loc");
            PRINT_TRACE (20, print_long, &i);
            if (0 < strcmp (ip->phrase_comp[i], token))
                (void) sprintf (phrase_buf, "%.100s %.100s",
                                token, ip->phrase_comp[i]);
            else
                (void) sprintf (phrase_buf, "%.100s %.100s",
                                ip->phrase_comp[i], token);
            if (UNDEF == add_con (ip,
                                  phrase_buf,
                                  &ip->parse_func[SM_PT_F_PHRASE],
                                  token_num,
                                  1))
                return (UNDEF);
        }
    }

    if (UNDEF == add_comp (ip, token, token_num))
        return (UNDEF);

    return (1);
}

/* Add token to list of possible phrase components occurring in
   current sentence.  */
static int
add_comp (ip, token, token_num)
STATIC_INFO *ip;
char *token;
long token_num;
{
    long i;
    long min_index, min_token_num;

    PRINT_TRACE (8, print_string, "Trace: entering parse_adj.add_comp");
    PRINT_TRACE (8, print_string, token);

    if (1 == ip->phrase_distance) {
        ip->phrase_comp[0] = token;
        ip->comp_loc[0] = token_num;
        ip->num_phrase_comp = 1;
        PRINT_TRACE (20, print_string, "token added at location 0");
        PRINT_TRACE (8, print_string, "Trace: leaving parse_adj.add_comp");
        return (0);
    }

    if (ip->num_phrase_comp < ip->phrase_distance) {
        ip->phrase_comp[ip->num_phrase_comp] = token;
        ip->comp_loc[ip->num_phrase_comp] = token_num;
        ip->num_phrase_comp++;
        return (0);
    }

    if (0 == ip->phrase_distance)
        return (0);

    /* Must find token loc furthest away from current loc, and replace it */
    min_token_num = ip->comp_loc[0]; min_index = 0;
    for (i = 1; i < ip->phrase_distance; i++) {
        if (ip->comp_loc[i] < min_token_num) {
            min_token_num = ip->comp_loc[i];
            min_index = i;
        }
    }
    ip->phrase_comp[min_index] = token;
    ip->comp_loc[min_index] = token_num;

    return (0);
}
