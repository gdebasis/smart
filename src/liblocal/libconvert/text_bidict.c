#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/pnorm_coll.c,v 11.2 1993/02/03 16:50:51 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 
 *
 * In this file, we read in a text dictionary file of the following format:
 * <word in source language> <words in target language>
 *
 * The words in the text fblocal/libconvert/text_bidict.cile are not stemmed and may contain words
 * which don't exist in the source or target collections.
 * We output the following:
 * an in-memory hash table for the stemmed words which exist in either
 * collection.
 *
 * We thus build up a dictionary which can directly be used for CLIR
 * from an external resource file.
*/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "docindex.h"
#include "context.h"
#include "spec.h"
#include "trace.h"
#include "dict.h"
#include "textline.h"
#include "uthash.h"
#include "wt.h"

#define MAX_LINE_SIZE 1024

static PROC_INST src_stemmer, tgt_stemmer;
static char *text_file;
static char *src_dict_file, *tgt_dict_file;
static FILE* text_fd;
static int src_dict_fd, tgt_dict_fd;
static long dict_mode;

static WordTranslation* tlTable = NULL;		// the in-memory hash table for storing the bi-lingual dictionary
static WordTranslation* tlConTable = NULL;	// the in-memory hash table for storing the bi-lingual dictionary keyed by con.
static WordTranslation* translationBuff;
static long translationBuffSize;

static SPEC_PARAM spec_args[] = {
    {"local.text_bdict.text_file",	getspec_string, (char *) &text_file},
    {"local.text_bdict.src_dict_file",	getspec_dbfile, (char *) &src_dict_file},
    {"local.text_bdict.tgt_dict_file",	getspec_dbfile, (char *) &tgt_dict_file},
    {"local.text_bdict.dict_file.rmode", getspec_filemode,(char *) &dict_mode},
    {"local.text_bdict.num_words",	getspec_long, (char *) &translationBuffSize},
    {"local.text_bdict.src_stemmer",	getspec_func, (char *) &src_stemmer.ptab},
    {"local.text_bdict.tgt_stemmer",	getspec_func, (char *) &tgt_stemmer.ptab},
    TRACE_PARAM ("local.text_bdict.index.trace")
};

static int num_spec_args = sizeof(spec_args) / sizeof(spec_args[0]);

int buildTranslationTable();
int saveInDict(WordTranslation* wtp);

int
init_text_bdict (spec)
SPEC *spec;
{
	int status;
    PRINT_TRACE (2, print_string, "Trace: entering init_text_bdict");
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    if (!VALID_FILE (text_file))
		return UNDEF;

    if (NULL == (text_fd = fopen (text_file, "r")))
		return (UNDEF);

	if (UNDEF == (src_dict_fd = open_dict (src_dict_file, dict_mode)) ||
		UNDEF == (tgt_dict_fd = open_dict (tgt_dict_file, dict_mode)))
		return UNDEF;

	translationBuff = Malloc (translationBuffSize, WordTranslation);
	if (translationBuff == NULL) {
		fprintf(stderr, "Unable to allocate memory for storing word translations");
		return UNDEF;
	}

    if (UNDEF == (src_stemmer.inst = src_stemmer.ptab->init_proc (spec, (char *)NULL))||
		UNDEF == (tgt_stemmer.inst = tgt_stemmer.ptab->init_proc (spec, (char *)NULL)))
		return (UNDEF);

	status = buildTranslationTable();
	if (UNDEF == status)
		return UNDEF;

	// Deallocate resources
	// close file handles
	fclose(text_fd);
	close_dict(src_dict_fd);
	close_dict(tgt_dict_fd);

	// close stemmer objects
    if (UNDEF == src_stemmer.ptab->close_proc (src_stemmer.inst) ||
		UNDEF == tgt_stemmer.ptab->close_proc (tgt_stemmer.inst) )
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_text_bdict");
    return (0);
} /* init_text_bdict */

int text_bdict (char* srcword, WordTranslation** wtp)
{
	// Lookup the hash table and return a pointer to the translations
	// for a given source language word
	HASH_FIND_STR(tlTable, srcword, (*wtp));
	return (*wtp == NULL)? UNDEF: 0;
}

int getTranslation (long con, WordTranslation** wtp)
{
	// Lookup the hash table and return a pointer to the translations
	// for a given source language word
    HASH_FIND(hh2, tlConTable, &con, sizeof(long), (*wtp));	
	return (*wtp == NULL)? UNDEF: 0;
}

int buildTranslationTable()
{
	SM_TEXTLINE textline;
	static char buff[MAX_LINE_SIZE];
	int i;
	char** tokens;
	WordTranslation *wtp;
   
	wtp	= translationBuff;

    PRINT_TRACE (2, print_string, "Trace: entering text_bdict");

	/* Read from the file and load an in-memory hash table
	 * for the bilingual dictionary. */
    textline.max_num_tokens = MAX_NUM_WORDS;
	tokens = Malloc(textline.max_num_tokens, char*);
	textline.tokens = tokens;

   	while (NULL != fgets(buff, sizeof(buff), text_fd) && wtp < &translationBuff[translationBuffSize]) {
        text_textline (buff, &textline);
		// save the token strings in the current entry of the WordTranslation object buffer
		for (i = 0; i < textline.num_tokens; i++) {
			// Preprocess the cases where one word in a source
			// language is translated into multiple words in the target language.
			// Remove the subsequent translated words in such cases.
			strncpy(wtp->words[i], textline.tokens[i], sizeof(wtp->words[i]));
		}
		wtp->numTranslations = textline.num_tokens;
		if (saveInDict(wtp) == UNDEF)
			return UNDEF;
		wtp++;	// point to the next object in the buffer...
	}

	free(tokens);
    PRINT_TRACE (2, print_string, "Trace: leaving text_bdict");
    return 0;
} /* text_bdict */


int close_text_bdict ()
{
	WordTranslation* p;
    PRINT_TRACE (2, print_string, "Trace: entering close_text_bdict");

	HASH_CLEAR(hh, tlTable);
	HASH_CLEAR(hh2, tlConTable);

	// Free the allocated memory for word translations
	free(translationBuff);

    PRINT_TRACE (2, print_string, "Trace: leaving close_text_bdict");
    return (0);
} /* close_text_bdict */

// Apply stemming and vocabulary check before saving in the in-memory dictionary...
int saveInDict(WordTranslation* wtp) {
	char *stemmedWord, *w;
	DICT_ENTRY dict;
	int status, i;

	w = wtp->words[0];
    if (UNDEF == src_stemmer.ptab->proc(w, &stemmedWord, src_stemmer.inst)) {
          fprintf(stderr,"LOOKUP: erroneous stem option setting for source language - quit\n");
		  return UNDEF;
	}
	// check if the word is in the vocabulary, if not discard it
    dict.token = stemmedWord;
	PRINT_TRACE(4, print_string, stemmedWord);

    dict.con = UNDEF;
    if (UNDEF == (status = (seek_dict (src_dict_fd, &dict))))
		return (UNDEF);
	if (status == 0) {
		// This word from the external source dictionary does not exist
		// in the vocabulary. So there is no point of loading it for our purpose.
		return 0;
	}
	else {
		// the word is dictionary
        if (1 != read_dict (src_dict_fd, &dict))
	        return (UNDEF);
	}

	wtp->cons[0] = dict.con;

	for (i = 1; i < wtp->numTranslations; i++) {
		w = wtp->words[i];
	    if (UNDEF == tgt_stemmer.ptab->proc(w, &stemmedWord, tgt_stemmer.inst)) {
			fprintf(stderr,"LOOKUP: erroneous stem option setting for target language - quit\n");
			return UNDEF;
		}

		PRINT_TRACE(4, print_string, stemmedWord);

	    dict.token = stemmedWord;
		dict.con = UNDEF;
	    if (UNDEF == (status = (seek_dict (tgt_dict_fd, &dict))))
			return (UNDEF);
		if (status == 0)
	   		return 0;
		else {
        	if (1 != read_dict (tgt_dict_fd, &dict))
	        	return (UNDEF);
		}
		wtp->cons[i] = dict.con;
    }

	// insert in hash table
	HASH_ADD_STR(tlTable, words[0], wtp);
	HASH_ADD(hh2, tlConTable, cons[0], sizeof(long), wtp);
	return 0;
}

