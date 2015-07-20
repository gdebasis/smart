#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/hn_triestem.c,v 11.2 1993/02/03 16:51:08 smart Exp $";
#endif

#include <locale.h>
#include <langinfo.h>
#include <wchar.h>
#include <wctype.h>
#include <string.h>
#include "hn_unicode.h"

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Determine stem of word by looking up stems in trie
 *1 index.stem.hn_triestem
 *2 hn_triestem (word, output_word, inst)
 *3   char *word;
 *3   char **output_word;
 *3   int inst;
 *4 init_hn_triestem (spec, param_prefix)
 *5    "index.stem.trace"
 *4 close_hn_triestem (inst)

 *7 Determine longest matching legal stem of word by looking up possible
 *7 stems in a trie, and then remove stem, returning resulting recoded root in
 *7 output_word.
 *7 Return UNDEF if error, 0 otherwise.

 *8 Basic algorithm from Lovins' (CACM) article, but has been much distorted
 *8 over the years.  Initial trie contains list of stems plus conditions under
 *8 which stem can NOT be applied.  Search that trie for the longest matching
 *8 stem that can be applied.  Remove the stem, and then recode the root to
 *8 normalize that form (eg want "believes" -> "belief").  Recoding done by
 *8 looking up end of root in a separate recoding trie.

 *9 Bug: Since tries are not yet full objects in the smart system, the tries
 *9 searched are compiled into the code.  This is bad.
 *9 Warning: assumes input word is already all lower case.
***********************************************************************/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "trie.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include <locale.h>
#include <langinfo.h>

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("index.stem.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

char* stemHindiWord(wchar_t* word);

static int wstrlen(wchar_t* word)
{
	wchar_t *p ;
	for (p = word; *p; ++p) ;
	return p - word;
}

int
init_hn_triestem (spec, unused)
SPEC *spec;
char *unused;
{
    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args)) {
        return (UNDEF);
    }

    if (NULL == setlocale(LC_CTYPE, "hi_IN.utf8")) {
        print_error("init_hn_triestem", "couldn't set locale");
        return UNDEF;
    }
    else
        printf("Locale set to %s\n", nl_langinfo(CODESET));

    PRINT_TRACE (2, print_string, "Trace: entering/leaving init_hn_triestem");

	return (0);
}

/* tstemmer uses a trie based approach */
int
hn_triestem (word, output_word, inst)
char *word;                     /* word to be stemmed */
char **output_word;             /* the stemmed word (with tstemmer, always
                                   the same as word ) */
int inst;
{
	wchar_t   hnWord[MAX_TOKEN_LEN];

    PRINT_TRACE (2, print_string, "Trace: entering hn_triestem");
    PRINT_TRACE (6, print_string, word);

	// Convert the multibyte sequence into wide character string
	mbstowcs(hnWord, word, MAX_TOKEN_LEN) ;
    
	*output_word = stemHindiWord(hnWord);

    PRINT_TRACE (4, print_string, *output_word);
    PRINT_TRACE (2, print_string, "Trace: leaving hn_triestem");

    return (0);
}

int
close_hn_triestem (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering/leaving close_hn_triestem");
    return (0);
}

typedef enum { false, true } bool ;

#define stripHn(word, len, pos)       \
    if (len-pos >= 3) {           \
        word[len - pos] = 0;        \
        len -= pos ;                \
    }

bool isHindiMatra(wchar_t a) {
    return (a >= hn_AA && a <= hn_AU);
}

bool isHindiCommonSuffix(wchar_t a) {
    if (a >= hn_AA && a <= hn_AU)
        return true;
    if (a >= hn_aa && a <= hn_au)
        return true;
    return a == hn_anusvara || a == hn_y || a == hn_chandrabindu? true:false;
}

char* stemHindiWord(wchar_t* word)
{
    wchar_t *p;
	static char buff[MAX_TOKEN_LEN * sizeof(wchar_t)];
    int len = wstrlen(word) ;

#if 0	
   	// Strip off "tA"
	if ( word[len-1] == hn_AA && word[len-2] == hn_t) {
       	stripHn(word, len, 2);
	}
#endif	

   	// Apply the simple rule of removing vowels including matras and anusvara
	// from the right side unless the first consonant character is hit.
   	p = &word[len-1];
	while (isHindiCommonSuffix(*p))
       	p--;

   	if (p-word+1 >= 2) *(p+1) = 0;  // if the remaining word is of atleast two characters apply stemming

	/*
    	// Strip off the suffix "on"
		else if ( word[len-1] == hn_anusvara && (word[len-2] == hn_o || word[len-2] == hn_O)) {
        	stripHn(word, len, 2) ;
		}
    	// Strip off the suffix "en"
		else if ( word[len-1] == hn_anusvara && (word[len-2] == hn_e || word[len-2] == hn_E)) {
        	stripHn(word, len, 2) ;
		}
    	// Strip off the suffix "yon"
		else if ( word[len-1] == hn_anusvara && (word[len-2] == hn_o || word[len-2] == hn_O) && word[len-3] == hn_y) {
        	stripHn(word, len, 3);
		}
    	// Strip off the suffix "yo"
		else if ( word[len-1] == hn_O && word[len-2] == hn_y) {
        	stripHn(word, len, 2);
		}

		// Finally drop off any remaining vowel
		else if (isHindiMatra(word[len-1])) {
        	stripHn(word, len, 1);
		}
	*/

	wcstombs(buff, word, MAX_TOKEN_LEN * sizeof(wchar_t)); 
	return buff;
}

