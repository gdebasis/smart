#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/stem_phrase.c,v 11.2 1993/02/03 16:51:19 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Stem and normalize a word phrase
 *1 index.stem.phrase
 *2 stem_phrase (word, output_word, inst)
 *3   char *word;
 *3   char **output_word;
 *3   int inst;
 *4 init_stem_phrase (spec, param_prefix)
 *5    "index.stem.trace"
 *4 close_stem_phrase (inst)

 *7 Take two tokens separated by a single blank, stem each
 *7 token in turn (using triestem()), and form a new phrase composed
 *7 of the stems in alphabetic order.
***********************************************************************/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "trie.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "inst.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("index.stem.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

typedef struct {
    /* bookkeeping */
    int valid_info;

    int stem_inst;
    char *stemmed_phrase;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_stem_phrase (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;

    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_stem_phrase");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];

    if (UNDEF == (ip->stem_inst = init_triestem (spec, (char *) NULL)))
        return (UNDEF);
    if (NULL == (ip->stemmed_phrase = malloc (2 * MAX_TOKEN_LEN)))
        return (UNDEF);

    ip->valid_info = 1;
        
    PRINT_TRACE (2, print_string, "Trace: leaving init_stem_phrase");

    return (new_inst);
}

int
stem_phrase (word, output_word, inst)
char *word;                     /* phrase to be stemmed */
char **output_word;             /* the stemmed word */
int inst;
{
    STATIC_INFO *ip;
    char *ptr, *word1, *stem1, *word2, *stem2;

    PRINT_TRACE (2, print_string, "Trace: entering stem_phrase");
    PRINT_TRACE (6, print_string, word);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "stem_phrase");
        return (UNDEF);
    }

    ip  = &info[inst];

    ptr = word;
    word1 = ptr;
    while (*ptr && *ptr != ' ')
        ptr++;
    if (*ptr == '\0') {
        set_error (SM_ILLPA_ERR, word, "stem_phrase");
        return (UNDEF);
    }
    *ptr = '\0';
    ptr++;
    word2 = ptr;
    if (UNDEF == triestem (word1, &stem1, ip->stem_inst) ||
        UNDEF == triestem (word2, &stem2, ip->stem_inst))
        return (UNDEF);

    /* Return pair in alphabetic order */
    if (0 > strcmp (stem1, stem2)) {
	(void) strcpy (ip->stemmed_phrase, stem1);
	ptr = &ip->stemmed_phrase[strlen(ip->stemmed_phrase)];
	*ptr++ = ' ';
	(void) strcpy (ptr, stem2);
    }
    else {
	(void) strcpy (ip->stemmed_phrase, stem2);
	ptr = &ip->stemmed_phrase[strlen(ip->stemmed_phrase)];
	*ptr++ = ' ';
	(void) strcpy (ptr, stem1);
    }
    *output_word = ip->stemmed_phrase;

    PRINT_TRACE (4, print_string, *output_word);
    PRINT_TRACE (2, print_string, "Trace: leaving stem_phrase");

    return (0);
}

int
close_stem_phrase (inst)
int inst;
{
    STATIC_INFO *ip;
    PRINT_TRACE (2, print_string, "Trace: entering close_stem_phrase");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_stem_phrase");
        return (UNDEF);
    }

    ip  = &info[inst];
    ip->valid_info--;

    if (ip->valid_info == 0) {
        if (UNDEF == close_triestem (ip->stem_inst))
            return (UNDEF);
        (void) free (ip->stemmed_phrase);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_stem_phrase");
    return (0);
}

