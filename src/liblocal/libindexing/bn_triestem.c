#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/stem_bangla.c,v 11.2 1993/02/03 16:51:05 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Simple stemming procedure for Bangla
 *1 local.index.stem.bangla
 *2 bn_triestem (word, output_word, inst)
 *3   char *word;
 *3   char **output_word;
 *3   int inst;
 *4 init_bn_triestem (spec, unused)
 *5   "index.stem.trace"
 *4 close_bn_triestem (inst)

 *7

 *9 Warning: Changes input parameter (stemming done in place).
***********************************************************************/

#include <locale.h>
#include <langinfo.h>
#include <wchar.h>
#include <wctype.h>
#include <string.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "bn_unicode.h"

typedef enum { false, true } bool ;

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("local.index.stem.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int wstrlen(wchar_t* word);
static bool isBengaliSwaraBarna(wchar_t a);
static bool isBengaliByanjanBarna(wchar_t a);
static void stripPluralSuffixes(wchar_t* word, int* len);
static bool stripCommonSuffixes(wchar_t* word, int* len, bool i_removed);
static char* stemWord(wchar_t* word);

int
init_bn_triestem (spec, unused)
SPEC *spec;
char *unused;
{
    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args)) {
        return (UNDEF);
    }
    if (NULL == setlocale(LC_CTYPE, "bn_IN.utf8")) {
        print_error("init_bn_triestem", "couldn't set locale");
        return UNDEF;
    }
    else
        printf("Locale set to %s\n", nl_langinfo(CODESET));


    PRINT_TRACE (2, print_string, "Trace: entering/leaving init_bn_triestem");
    return (0);
}

int
bn_triestem (word, output_word, inst)
char *word;                     /* word to be stemmed */
char **output_word;
int inst;
{
	wchar_t  bnWord[MAX_TOKEN_LEN];

    PRINT_TRACE (2, print_string, "Trace: entering bn_triestem");
    PRINT_TRACE (6, print_string, word);

	// Convert the multibyte sequence into wide character string
	mbstowcs(bnWord, word, MAX_TOKEN_LEN) ;
	*output_word = stemWord(bnWord);

    PRINT_TRACE (4, print_string, *output_word);
    PRINT_TRACE (2, print_string, "Trace: leaving bn_triestem");
    return (0);
}

int
close_bn_triestem (inst)
int inst;
{
    PRINT_TRACE (2,print_string, "Trace: entering/leaving close_bn_triestem");
    return (0);
}

static int wstrlen(wchar_t* word)
{
	wchar_t *p ;
	for (p = word; *p; p++) ;
	return p - word;
}

static bool isBengaliSwaraBarna(wchar_t a)
{
	static wchar_t swaraBarnas[] = { bn_AA, bn_E, bn_I, bn_II, bn_U, bn_UU } ;
	int i ;

	for (i = 0; i < sizeof(swaraBarnas)/sizeof(*swaraBarnas); i++) {
		if ( a == *(swaraBarnas + i) )
			return true ;
	}

	return false ; 
}

static bool isBengaliByanjanBarna(wchar_t a)
{
	return a >= bn_k && a <= bn_y ;
}

/* A common suffix candidate is a sequence of vowels and bn_y. */ 
static bool isBnCommonSuffix(wchar_t a) {
    if (a >= bn_AA && a <= bn_AU)
        return true;
    if (a >= bn_aa && a <= bn_au)
        return true;
    return a == bn_y? true:false;
}


// Strip off suffixes "gulo", "guli", "gulote" "gulite"
static void stripPluralSuffixes(wchar_t* word, int* len)
{
	if (*len <= 6)
		return ;

	if ( word[*len-1] == bn_E && word[*len-2] == bn_t ) {
		word[*len - 2] = 0 ;
		*len -= 2 ;
	}

	if (*len <= 6)
		return ;

	if ( word[*len-4] == bn_g && word[*len-3] == bn_U && word[*len-2] == bn_l &&
			(word[*len-1] == bn_O || word[*len-1] == bn_I) ) {
		word[*len - 4] = 0 ;
		*len -= 4 ;
	}
}

// Strip off suffixes like "rai", "tuku", "shil" "ta" etc.
static bool stripCommonSuffixes(wchar_t* word, int* len, bool i_removed)
{
	int newlen = *len ;

	do {
		if (*len <= 4)
			break ;

		// Remove 'ttA' or 'tA' (only if it is not preceeded with a m or g)
		if ( word[*len-1] == bn_AA &&
			(word[*len-2] == bn_tt || (word[*len-2] == bn_t &&
			 word[*len-3] != bn_m && word[*len-3] != bn_g) ))   {
			word[*len - 2] = 0 ;
			*len -= 2 ;
		}

		if (*len <= 4)
			break ;

		// Remove 'ti' or 'tti'
		if ( word[*len-1] == bn_I &&
			word[*len-2] == bn_tt )  {
			word[*len - 2] = 0 ;
			*len -= 2 ;
		}

		if (*len <= 4)
			break ;

		// Remove "ra"  ("rai" has alreday been stemmed to "ra").
		if (word[*len-1] == bn_r) { 	
			word[*len - 1] = 0 ;
			*len -= 1 ;

			if (*len <= 4)
				break ;

			// Remove "-er"
			if (word[*len-1] == bn_E) {
				int pos = word[*len-2] == bn_d ? 2 : 1; // Remove '-der'
				word[*len - pos] = 0 ;
				*len -= pos ;
			}
		}

		if (*len <= 5)
			break ;

		// Remove ttai tai ttar or tar
		if ( (word[*len-1] == bn_y || word[*len-1] == bn_r) &&
			word[*len-2] == bn_AA &&
			(word[*len-3] == bn_tt || word[*len-3] == bn_t)) {
			word[*len - 3] = 0 ;
			*len -= 3 ;
		}
		else if ( (word[*len-1] == bn_r) && word[*len-2] == bn_I && word[*len-3] == bn_tt) {
			word[*len - 3] = 0 ;
			*len -= 3 ;
		}

		if (*len <= 5)
			break ;

		if ( word[*len-1] == bn_E && word[*len-2] == bn_k) {
			word[*len - 2] = 0 ;
			*len -= 2 ;
		}

		if (*len <= 5)
			break;

		// Remove 'shil'
		if (word[*len-1] == bn_l && word[*len-2] == bn_II && word[*len-3] == bn_sh) {
			word[*len - 3] = 0 ;
			*len -= 3 ;
		}

		if (*len <= 6)
			break;

		// Remove 'tuku'
		if (word[*len-1] == bn_U && word[*len-2] == bn_k && word[*len-3] == bn_U && word[*len-4] == bn_tt) {
			word[*len - 4] = 0 ;
			*len -= 4 ;
		}

		// Remove 'debi'
		if (*len <= 6)
			break;

		if (word[*len-1] == bn_II && word[*len-2] == bn_b && word[*len-3] == bn_E && word[*len-4] == bn_d) {
			word[*len - 4] = 0 ;
			*len -= 4 ;
		}

		// Remove 'babu'
		if (*len <= 6)
			break;

		if (word[*len-1] == bn_U && word[*len-2] == bn_b && word[*len-3] == bn_AA && word[*len-4] == bn_b) {
			word[*len - 4] = 0 ;
			*len -= 4 ;
		}

		// Remove 'bhai'
		if (*len <= 6 || !i_removed)
			break;

		if (word[*len-1] == bn_AA && word[*len-2] == bn_bh) {
			word[*len - 2] = 0 ;
			*len -= 2 ;
		}

		// Remove 'bhabe'
		if (*len <= 6)
			break;

		if (word[*len-1] == bn_b && word[*len-2] == bn_E && word[*len-3] == bn_AA && word[*len-4] == bn_bh) {
			word[*len - 4] = 0 ;
			*len -= 4 ;
		}
	}
	while (0) ;

	return newlen != *len;
}

static bool suffixEndingByanjonBarna(wchar_t ch)
{
	return (ch == bn_d || ch == bn_k || ch == bn_tt || ch == bn_t || ch == bn_m);
}
 
static char* stemWord(wchar_t* word)
{
	static char buff[MAX_TOKEN_LEN * sizeof(wchar_t)];
    int len = wstrlen(word);
	bool i_removed;
	wchar_t *p;

	do {
		if (len <= 3)
			break;

		// Remove trailing okhyor "i" and "o"
		if (word[len-1] == bn_i || word[len-1] == bn_o) {
			word[len-1] = 0;
			len--;
			i_removed = true;
		}

		while (stripCommonSuffixes(word, &len, i_removed)) {
			i_removed = false;
		}

		stripPluralSuffixes(word, &len) ;

		/* Remove sequences of vowels. Allow atleast 4 letters to remain */
		p = word+len-1;
		while (isBnCommonSuffix(*p)) {
			p--;
		}
		if (p-word+1 >= 2)
			*(p+1) = 0;
	}
	while (0);

	wcstombs(buff, word, MAX_TOKEN_LEN * sizeof(wchar_t)); 
	return buff;
}

