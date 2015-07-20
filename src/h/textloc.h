#ifndef TEXTLOCH
#define TEXTLOCH
/*        $Header: /home/smart/release/src/h/textloc.h,v 11.2 1993/02/03 16:47:48 smart Exp $*/

typedef struct {
    long  id_num;                  /* Id of text */
    long begin_text;               /* Offset within file_name of text start */
    long end_text;                 /* text end as above (see below) */
    long doc_type;                 /* Collection dependent type of doc */
    char  *file_name;              /* File to find text in */
    char  *title;                  /* Title of text */
} TEXTLOC;

/*
 * NOTE: A text which consists of exactly two characters at the start
 *	 of a file would have begin_text=0 and end_text=2, so the
 *	 total size of the text is end_text - begin_text.
 */

#endif /* TEXTLOCH */
