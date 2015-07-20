#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/remove_s.c,v 11.2 1993/02/03 16:51:05 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 stemming procedure that just changes plural to singular
 *1 index.stem.remove_s
 *2 remove_s (word, output_word, inst)
 *3   char *word;
 *3   char **output_word;
 *3   int inst;
 *4 init_remove_s (spec, unused)
 *5   "index.stem.trace"
 *4 close_remove_s (inst)

 *7 Remove final 's' if indicating plural. Change final 'ies' to 'y'.
 *7 output_word points to stemmed word (actually always points to word,
 *7 which has been changed).
 *7 Always returns 0.

 *9 Warning: Changes input parameter.
***********************************************************************/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "trie.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("index.stem.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static char *remove_plural();

int
init_remove_s (spec, unused)
SPEC *spec;
char *unused;
{
    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering/leaving init_remove_s");

return (0);
}

int
remove_s (word, output_word, inst)
char *word;                     /* word to be stemmed */
char **output_word;             /* the stemmed word (with remove_s, always
                                   the same as word ) */
int inst;
{

    PRINT_TRACE (2, print_string, "Trace: entering remove_s");
    PRINT_TRACE (6, print_string, word);

    *output_word = remove_plural (word);

    PRINT_TRACE (4, print_string, *output_word);
    PRINT_TRACE (2, print_string, "Trace: leaving remove_s");

    return (0);
}

int
close_remove_s (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering/leaving close_remove_s");
    return (0);
}

static char *
remove_plural (word)
char *word;
{
    int len = strlen (word);
    
    if (len <= 2 || word[len-1] != 's')
        /* not a plural word; return */
        return (word);

    /* otherwise, a candidate plural word */
    switch(word[len-2]) {
      case 'e':    /* still 2 possibilites - 'es', 'ies' */
        if (len > 3 && word[len-3] == 'i' &&
            word[len-4] != 'e' && word[len-4] != 'a' ) {
            /* 'ies' -> 'y' */
            word[len-3] = 'y';
            word[len-2] = '\0';
        }
        else if (len>3 && word[len-3] != 'a' && word[len-3] != 'e'
                 && word[len-3] != 'i' && word[len-3] != 'o')
            /* 'es' -> 'e' */
            word[len-1] = '\0';
        break;
      case 'u':    /* do not remove 's' */
      case 's':
      case '\'':
        break;
      default:     /* remove final 's' */
        word[len-1] = '\0';
    }
    return(word);         
}
