#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/stem_no.c,v 11.2 1993/02/03 16:51:13 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Empty stemming procedure 
 *1 index.stem.none
 *2 stem_no (word, output_word, inst)
 *3   char *word;
 *3   char **output_word;
 *3   int inst;

 *7 No stemming done.  Output_word is set to word.
 *7 Always returns 0.
***********************************************************************/

int
stem_no (word, output_word, inst)
char *word;                     /* word to be stemmed */
char **output_word;             /* the stemmed word (with stem_no, always
                                   the same as word ) */
int inst;
{
    *output_word = word;
    return (0);
}
