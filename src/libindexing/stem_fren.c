#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/stem_french.c,v 11.2 1993/02/03 16:51:05 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Simple stemming procedure for French
 *1 index.stem.french
 *2 stem_french (word, output_word, inst)
 *3   char *word;
 *3   char **output_word;
 *3   int inst;
 *4 init_stem_french (spec, unused)
 *5   "index.stem.trace"
 *4 close_stem_french (inst)

 *7 Removes initial "l'", "d'", "s'", "n'", "qu'", "j'", "m'".
 *7 Removes final "s", "e", "es", "ment", "'s" (contamination?).
 *7 Changes final "if", "ifs" to "iv" (matches trimmed "ive").
 *7 Trims final "erai", "erons", "erez", "eront", "erais", "erait",
 *7 "erions", "eriez", "eraient" to "er", and same for "ir" forms.
 *7 ("era" and "eras" seem too dangerous)
 *7 Trims final "elle", "elles" to "el" and "enne", "ennes" to "en".
 *7 output_word points to stemmed word (actually always points within word,
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

static char *french();

int
init_stem_french (spec, unused)
SPEC *spec;
char *unused;
{
    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering/leaving init_stem_french");

return (0);
}

int
stem_french (word, output_word, inst)
char *word;                     /* word to be stemmed */
char **output_word;             /* the stemmed word (with stem_french, always
                                   the same as word ) */
int inst;
{

    PRINT_TRACE (2, print_string, "Trace: entering stem_french");
    PRINT_TRACE (6, print_string, word);

    *output_word = french (word);

    PRINT_TRACE (4, print_string, *output_word);
    PRINT_TRACE (2, print_string, "Trace: leaving stem_french");

    return (0);
}

int
close_stem_french (inst)
int inst;
{
    PRINT_TRACE (2,print_string, "Trace: entering/leaving close_stem_french");
    return (0);
}

static char *
french (word)
char *word;
{
    int len = strlen (word);

    if (len <= 2)
        /* not a candidate word; return */
        return (word);

    /* trim beginning of word */
    switch (word[0]) {
      case 'd':
      case 'j':
      case 'l':
      case 'm':
      case 'n':
      case 's':
	if (word[1] == '\'') {
	    word += 2;
	    len -= 2;
	    if (len <= 2) return (word);
	}
	break;
      case 'q':
	if (word[1] == 'u') {
	    if (word[2] == '\'') {
		word += 3;
		len -= 3;
		if (len <= 2) return (word);
	    }
	}
	break;
      default:
	break;
    }

    /* trim end of word */
    switch (word[len-1]) {
      case 'f':
        {
            switch (word[len-2]) {
              case 'i':
                word[len-1] = 'v';
                break;
              default:
                break;
            }
        }
        break;
      case 'e':
        {
            switch (word[len-2]) {
              case 'l':
		if (len >= 6 && !strcmp(&word[len-4], "elle"))
		    word[len-2] = '\0';
		else
		    word[len-1] = '\0';
                break;
              case 'n':
		if (len >= 6 && !strcmp(&word[len-4], "enne"))
		    word[len-2] = '\0';
		else
		    word[len-1] = '\0';
                break;
              default:
		word[len-1] = '\0';
                break;
            }
        }
        break;
      case 's':
        {
            switch (word[len-2]) {
              case '\'':
                word[len-2] = '\0';
                break;
              case 'e':
		if (len >= 7 && (!strcmp(&word[len-5], "ennes") ||
					    !strcmp(&word[len-5], "elles")))
		    word[len-3] = '\0';
		else
		    word[len-2] = '\0';
		break;
              case 'f':
		if (word[len-3] == 'i')
		    word[len-2] = 'v';
		word[len-1] = '\0';
                break;
	      case 'i':
		if (len >= 7 && (!strcmp(&word[len-5], "erais") ||
					    !strcmp(&word[len-5], "irais")))
		    word[len-3] = '\0';
		else
		    word[len-1] = '\0';
		break;
	      case 'n':
		if (len >= 8 && (!strcmp(&word[len-6], "erions") ||
					    !strcmp(&word[len-6], "irions")))
		    word[len-4] = '\0';
		else if (len >= 7 && (!strcmp(&word[len-5], "erons") ||
					    !strcmp(&word[len-5], "irons")))
		    word[len-3] = '\0';
		else
		    word[len-1] = '\0';
		break;
              default:
                word[len-1] = '\0';
            }
        }
        break;
      case 't':
        {
	    if (len < 7) break;
            switch (word[len-2]) {
              case 'n':
		if (len >= 9 && (!strcmp(&word[len-7], "eraient") ||
					    !strcmp(&word[len-7], "iraient")))
		    word[len-5] = '\0';
		else if (!strcmp(&word[len-5], "eront") ||
					    !strcmp(&word[len-5], "iront"))
		    word[len-3] = '\0';
		else if (!strcmp(&word[len-4], "ment"))
		    word[len-4] = '\0';
                break;
              case 'i':
		if (!strcmp(&word[len-5], "erait") ||
					    !strcmp(&word[len-5], "irait"))
		    word[len-3] = '\0';
                break;
	      default:
	      	break;
	    }
	}
	break;
      case 'i':
        {
            switch (word[len-2]) {
              case 'a':
		if (len >= 6 && (!strcmp(&word[len-4], "erai") ||
					    !strcmp(&word[len-4], "irai")))
		    word[len-2] = '\0';
                break;
              default:
                break;
            }
	}
	break;
      case 'z':
        {
            switch (word[len-2]) {
              case 'e':
		if (len >= 7 && (!strcmp(&word[len-5], "eriez") ||
					    !strcmp(&word[len-5], "iriez")))
		    word[len-3] = '\0';
		else if (len >= 6 && (!strcmp(&word[len-4], "erez") ||
					    !strcmp(&word[len-4], "irez")))
		    word[len-2] = '\0';
                break;
              default:
                break;
            }
	}
	break;
      default:
        break;
    }
    return(word);         
}
