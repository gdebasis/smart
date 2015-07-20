#ifndef __BILINGUAL_DICT_H
#define __BILINGUAL_DICT_H

// Note: the maximum number of translations is in fact one less than MAX_NUM_WORDS
#define MAX_NUM_WORDS 8
#define MAX_WORD_SIZE 64

// The structure for storing the word translations.
// The first word contains the source word followed by translations in the
// target language.
typedef struct {
	char words[MAX_NUM_WORDS][MAX_WORD_SIZE];   // for storing all possible translations in target language
	long cons[MAX_NUM_WORDS];					// store the indexed concept numbers
	int  numTranslations;						// number of translations
    UT_hash_handle hh;                          // hashable by the source field
    UT_hash_handle hh2;                         // hashable by the source field
} WordTranslation;

extern int init_text_bdict (SPEC *spec);
extern int text_bdict (char* srcword, WordTranslation** wtp);
extern int close_text_bdict ();

#endif
