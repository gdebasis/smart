#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/stem_spanish.c,v 11.2 1993/02/03 16:51:05 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Simple stemming procedure for Spanish
 *1 index.stem.spanish
 *2 stem_spanish (word, output_word, inst)
 *3   char *word;
 *3   char **output_word;
 *3   int inst;
 *4 init_stem_spanish (spec, unused)
 *5   "index.stem.trace"
 *4 close_stem_spanish (inst)

 *7 Removes final "as", "es", "os", "a", "o", "e".
 *7 Changes final "z" to "c".
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

static char *spanish();

int
init_stem_spanish (spec, unused)
SPEC *spec;
char *unused;
{
    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering/leaving init_stem_spanish");

return (0);
}

int
stem_spanish (word, output_word, inst)
char *word;                     /* word to be stemmed */
char **output_word;             /* the stemmed word (with stem_spanish, always
                                   the same as word ) */
int inst;
{

    PRINT_TRACE (2, print_string, "Trace: entering stem_spanish");
    PRINT_TRACE (6, print_string, word);

    *output_word = spanish (word);

    PRINT_TRACE (4, print_string, *output_word);
    PRINT_TRACE (2, print_string, "Trace: leaving stem_spanish");

    return (0);
}

int
close_stem_spanish (inst)
int inst;
{
    PRINT_TRACE (2,print_string, "Trace: entering/leaving close_stem_spanish");
    return (0);
}

static char *
spanish (word)
char *word;
{
    int len = strlen (word);

    if (len > 2 && word[len-1] == 'z') {
        word[len-1] = 'c';
        return (word);
    }
    if (len <= 2)
        /* not a candidate word; return */
        return (word);

    switch (word[len-1]) {
      case 'e':
      case 'a':
      case 'o':
        word[len-1] = '\0';
        break;
      case 's':
        {
            switch (word[len-2]) {
              case 'e':
              case 'a':
              case 'o':
                word[len-2] = '\0';
                break;
              default:
                word[len-1] = '\0';
            }
        }
        break;
      default:
        break;
    }
    return(word);         
}
