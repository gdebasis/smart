#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/stem_ext.c,v 11.2 1993/02/03 16:51:05 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Simple stemming procedure for Bangla
 *1 local.index.stem.bangla
 *2 stem_ext (word, output_word, inst)
 *3   char *word;
 *3   char **output_word;
 *3   int inst;
 *4 init_stem_ext (spec, unused)
 *5   "local.index.stem.stem_file"
 *5   "local.index.stem.trace"
 *4 close_stem_ext (inst)

 *7

 *9 Warning: assumes stem_file is in proper format (two columns, separated 
 *9 by whitespace, ending in newline)
 *9 Warning: Does not do stemming in place. This may be at variance with the 
 *9 strategy followed by other SMART stemmers.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "stem.h"

static char *stem_file;
static SPEC_PARAM spec_args[] = {
    {"local.index.stem.stem_file", getspec_dbfile, (char *) &stem_file},
    TRACE_PARAM ("local.index.stem.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int max_inst;

static char *inbuf;
static int num_words;
static FILE *stem_fp;
static WORD_STEM *word_stem_list;
static int comp_word(const void *, const void *);

int
init_stem_ext (spec, unused)
SPEC *spec;
char *unused;
{
    char *cp;
    long filelen;

    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_stem_ext");

    if (max_inst > 0) /* initialization has already been done */
        return max_inst;
    max_inst++;

    /* read in stem file */
    if (! VALID_FILE (stem_file) ||
        NULL == (stem_fp = fopen(stem_file, "r")))
        return (UNDEF);
    if (UNDEF == fseek(stem_fp, 0, SEEK_END) ||
        UNDEF == (filelen = ftell(stem_fp))) {
        set_error(errno, "Problem determining file size", "init_stem_ext");
        return UNDEF;
    }
    rewind(stem_fp);

    if (NULL == (inbuf = Malloc(filelen, char))) {
        set_error(errno, "Out of memory", "init_stem_ext");
        return UNDEF;
    }
    if (filelen != fread(inbuf, sizeof(char), filelen, stem_fp)) {
        set_error(errno, "Problem reading file", "init_stem_ext");
        return UNDEF;
    }
    fclose(stem_fp);

    /* set up <original word, stemmed form> array */
    for (cp = inbuf; cp < inbuf + filelen; cp++)
        if (*cp == '\n') num_words++;

	num_words = num_words >> 1 ;
    if (NULL == (word_stem_list = Malloc(num_words, WORD_STEM))) {
        set_error(errno, "Out of memory", "init_stem_ext");
        return UNDEF;
    }
    for (cp = inbuf, num_words = 0; cp < inbuf + filelen; num_words++) {
        /* original word */
        word_stem_list[num_words].original = cp;
        while (!isspace(*cp) && cp < inbuf + filelen) cp++;
        *cp++ = '\0';
        /* separating white space */
        while (isspace(*cp) && cp < inbuf + filelen) cp++;
        /* stemmed word */
        word_stem_list[num_words].stemmed = cp;
        while (!isspace(*cp) && cp < inbuf + filelen) cp++;
        *cp++ = '\0';
        while (cp < inbuf + filelen && isspace(*cp)) cp++;
    }
    qsort(word_stem_list, num_words, sizeof(WORD_STEM), comp_word);

    PRINT_TRACE (2, print_string, "Trace: leaving init_stem_ext");
    return (0);
}

int
stem_ext (word, output_word, inst)
char *word;                     /* word to be stemmed */
char **output_word;
int inst;
{
    WORD_STEM key, *ptr;

    PRINT_TRACE (2, print_string, "Trace: entering stem_ext");
    PRINT_TRACE (6, print_string, word);

    key.original = word;
    if (NULL == (ptr = bsearch((const void *)&key, word_stem_list, num_words,
                               sizeof(WORD_STEM), comp_word)))
        *output_word = word;
    else *output_word = ptr->stemmed;

    PRINT_TRACE (4, print_string, *output_word);
    PRINT_TRACE (2, print_string, "Trace: leaving stem_ext");
    return (0);
}

int
close_stem_ext (inst)
int inst;
{
    PRINT_TRACE (2,print_string, "Trace: entering close_stem_ext");

    if (inst == 0) { /* only for the first (real) initialization */
        free(inbuf);
        free(word_stem_list);
    }

    PRINT_TRACE (2,print_string, "Trace: leaving close_stem_ext");
    return (0);
}


static int
comp_word(const void *p1, const void *p2)
{
    WORD_STEM *wsp1 = (WORD_STEM *)p1, *wsp2 = (WORD_STEM *)p2;

    return(strcmp(wsp1->original, wsp2->original));
}
