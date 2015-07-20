#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/stop_no.c,v 11.2 1993/02/03 16:51:14 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Empty stopword procedure
 *1 index.stop.stop_no
 *2 stop_no (word, stopword_flag, inst)
 *3   char *word;
 *3   int *stopword_flag;
 *3   int inst;

 *7 Always set stopword_flag to 0, indicating that word is not a stop word
 *7 (ie, not a common word to be removed during indexing).
 *7 Always return 0;

***********************************************************************/

int
stop_no (word, stopword_flag, inst)
char *word;                     /* word to be checked if stopword */
long *stopword_flag;
int inst;
{
    *stopword_flag = 0;
    return (0);
}
