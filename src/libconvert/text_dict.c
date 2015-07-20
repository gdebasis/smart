#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/text_dict.c,v 11.2 1993/02/03 16:49:31 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Convert a text form of single dictionary tuple into DICT_ENTRY format
 *1 convert.tup.text_dict
 *2 text_dict (text, dict, inst)
 *3   SM_BUF *text;
 *3   DICT_ENTRY *dict;
 *3   int inst;
 *4 init_text_dict (spec, unused)
 *4 close_text_dict (inst)
 *7 Read a text line from "text" of the form
 *7     token\tinfo\tcon
 *7 where \t indicates a tab, leading spaces are ignored, and only as many 
 *7 fields as are on the text line are filled in.  Con is always ignored.
 *7 Return 0 if "text" does not have a token, return 1 otherwise.
***********************************************************************/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "dict.h"
#include "buf.h"

/* Convert the given text_entry to a dictionary form */

int
init_text_dict (spec, unused)
SPEC *spec;
char *unused;
{
    return (1);
}

int
text_dict (text, dict, inst)
SM_BUF *text;
DICT_ENTRY *dict;
int inst;
{
    char *ptr;

    if (text == NULL || text->end == 0)
        return (0);
    for (ptr = text->buf; ptr < &text->buf[text->end] && isspace (*ptr); ptr++)
        ;
    if ( ptr >= &text->buf[text->end])
        return (0);
    dict->token = ptr;
    dict->info = 0;
    dict->con = UNDEF;
    for (ptr = text->buf; ptr < &text->buf[text->end] && *ptr != '\t'; ptr++)
        ;
    *ptr = '\0';
    if ( ptr >= &text->buf[text->end])
        return (1);
    /* Next field is info if it exists */
    ptr++;
    dict->info = atol (ptr);
    for (ptr = text->buf; ptr < &text->buf[text->end] && *ptr != '\t'; ptr++)
        ;
    if (ptr >= &text->buf[text->end])
        return (1);

    /* If there's another field, it is a concept, and is ignored.  Concept
       will be set by seek_dict and write_dict in the calling procedure */

    return (1);
}

int
close_text_dict (inst)
int inst;
{
    return (0);
}

