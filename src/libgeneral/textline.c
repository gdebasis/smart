#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libgeneral/textline.c,v 11.2 1993/02/03 16:50:34 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Utility procedure to tokenize a line
 *2 text_textline (string, textline)
 *3  char *string;
 *3  SM_TEXTLINE *textline;
 *7 String is broken up into an array of tokens (deliminated by space
 *7 characters as defined by isspace) returned in textline.
 *7 typedef struct {
 *7     char *text;
 *7     char **token;
 *7     long num_tokens;
 *7     long max_num_tokens;
 *7 } SM_TEXTLINE;
 *7 String must be a writable null-terminated string, and is changed by
 *7 text_textline (space characters replaced by '\0')
 *7 textline->token must be a preallocated array of length 
 *7 textline->max_num_tokens.  
 *7 textline->num_tokens is set to the number of tokens found on in string
 *7 (with a maximum value of textline->max_num_tokens).
 *7 textline->text is set to string on output.
 *7 Return UNDEF if string is NULL, 0 otherwise.
 *7 Used primarily by routines that convert a text file into some other
 *7 SMART object.
***********************************************************************/

#include <ctype.h>
#include "common.h"
#include "functions.h"
#include "textline.h"

int text_textline (char* string, SM_TEXTLINE* textline)
{
    char *ptr;

    if (string == NULL)
        return (UNDEF);
    textline->num_tokens = 0;
    textline->text = string;
    ptr = string;
    while (*ptr) {
        while (isspace (*ptr))
            ptr++;
        if (*ptr && textline->num_tokens < textline->max_num_tokens) {
            textline->tokens[textline->num_tokens] = ptr;
            textline->num_tokens++;
        }
        while (*ptr && !isspace (*ptr))
            ptr++;
        if (*ptr) {
            *ptr = '\0';
            ptr++;
        }
    }
    return (0);
}

int text_textline_delims (char* string, char* delims, SM_TEXTLINE* textline)
{
    char *ptr;

    if (string == NULL)
        return (UNDEF);
    textline->num_tokens = 0;
    textline->text = string;
    ptr = string;
    while (*ptr) {
		while (strchr(delims, *ptr))
			ptr++;
        if (*ptr && textline->num_tokens < textline->max_num_tokens) {
            textline->tokens[textline->num_tokens] = ptr;
            textline->num_tokens++;
        }
        while (*ptr && !isspace (*ptr))
            ptr++;
        if (*ptr) {
            *ptr = '\0';
            ptr++;
        }
    }
    return (0);
}
