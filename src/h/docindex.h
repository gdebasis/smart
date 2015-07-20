#ifndef DOCINDEXH
#define DOCINDEXH
/*  $Header: /home/smart/release/src/h/docindex.h,v 11.2 1993/02/03 16:47:27 smart Exp $ */

#include "eid.h"
#include "sm_display.h"
#include "textloc.h"
/* #include "proc.h" */
/* #include "vector.h" */

/* Used by preparser, adddisplay, parser to give the original text of the
   doc broken into sections */

typedef struct {
    EID id_num;
    char *doc_text;
    TEXTLOC textloc_doc;         /* section occurrence info relating to 
                                    actual text of doc in file */
    SM_DISPLAY mem_doc;          /* section occurrence info relating to
                                    in memory doc (ie doc_text) */
} SM_INDEX_TEXTDOC;

/* Output of tokenizer, giving the tokens found in the text of a doc, along 
   with the tokentype */
typedef struct {
    unsigned char *token;
    // wchar_t *wc_token; /* wide character equivalent of token when processing unicode input */
    long tokentype;
} SM_TOKENTYPE;                     /* Individual token */

typedef struct {
    long section_num;               /* Index of the section_id for section */
    long num_tokens;
    SM_TOKENTYPE *tokens;
} SM_TOKENSECT;                     /* Tokens for a section */

typedef struct {      
    long id_num;
    long num_sections;
    SM_TOKENSECT *sections;
} SM_TOKENDOC;                      /* Tokens for a document */

/* Old */
/* typedef struct { */
/*     long id_num; */
/*     long num_tokens; */
/*     SM_TOKENTYPE *tokens; */
/* } SM_INDEX_PARSEDOC; */

/* Tokentype can take on one of the following values in the standard parser */
/* Other parsers may use other values in addition.  By convention, positive
   values indicate that particular tokentype should be used as ctype */
/* Token is all lower case letters */
#define SM_INDEX_LOWER 0x0001
/* Token is mixed case (Combination of upper, lower, digits)  (possibly
   including '.', '!', ''' */
#define SM_INDEX_MIXED 0x0002
/* Token is mixed case (as above) + first letter is uppercase */
#define SM_INDEX_MIXED_FIRST_CAP 0x0010
/* Token is mixed case (as above) + non-initial letter is uppercase */
#define SM_INDEX_MIXED_OTHER_CAP 0x0020
/* Token is all digits (possibly including '.') */
#define SM_INDEX_DIGIT 0x0003
/* Punctuation */
#define SM_INDEX_PUNC 0x0004
/* Whitespace, not including newline */
#define SM_INDEX_SPACE 0x0005
/* Whitespace, including newline */
#define SM_INDEX_NLSPACE 0x0006
/* End of Sentence (Token = NULL) */
#define SM_INDEX_SENT 0x0007
/* End of Paragraph (Token = NULL) */
#define SM_INDEX_PARA 0x0008
/* Start of new section (Token[0] = section_id) */
#define SM_INDEX_SECT_START 0x0009
/* End of section (Token[0] = section_id) */
#define SM_INDEX_SECT_END 0x000a
/* Ignore this token completely */
#define SM_INDEX_IGNORE 0x000b


/* Output of parser */
typedef struct {
    long con;                   /* Concept number of input token */
    long ctype;                 /* Ctype of con */
    long para_num;              /* Paragraph within section */
    long sent_num;              /* Sentence number within section */
    long token_num;             /* Token number within section */
} SM_CONLOC;

typedef struct {
    long num_tokens_in_section; /* The actual number of tokens in the section,
                                   whether assigned a concept or not */
    long num_concepts;
    SM_CONLOC *concepts;
} SM_CONSECT;

typedef struct {
    long id_num;
    long num_sections;
    SM_CONSECT *sections;
} SM_CONDOC;


/* Old */
/* typedef struct { */
/*     long con;                   * Concept number of input token * */
/*     long ctype;                 * Ctype of con * */
/*     long sect_num;              * Section within document * */
/*     long para_num;              * Paragraph within document * */
/*     long sent_num;              * Sentence number within document * */
/*     long word_num;              * Word number within document * */
/* } SM_INDEX_CONLOC; */
/*  */
/* typedef struct { */
/*     long id_num; */
/*     long num_concepts; */
/*     SM_INDEX_CONLOC *concepts; */
/* } SM_INDEX_CONDOC; */

/* #include "docdesc.h" */
/* intermediate structure used to return token map from section parser to
   parser - OLD */
/* typedef struct { */
/*     SM_TOKENTYPE *last_token;   * Last token looked at to get new token map * */
/*     char *token_buf;            * Actual token to be operated on * */
/*     int parse_type;             * Index in std_parsetypes of the type of */
/*                                   token recognized * */
/*     SM_INDEX_CONLOC *conloc;    * Place for section parser to put */
/*                                    location info */ 
/* } SM_SEC_PARSE; */

#endif /* DOCINDEXH */
