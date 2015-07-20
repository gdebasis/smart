/* static char rcsid[] = "$Header: /home/smart/release/src/h/preparser.h,v 11.2 1993/02/03 16:47:37 smart Exp $"; */

#ifndef PREPARSERH
#define PREPARSERH

#include "docindex.h"

#define DISCARD_SECTION '-'

/* Globally available procedures to copy/discard a section of text 
 * (see pp_gen_line.c for definitions) 
 */
extern char *pp_copy    _AP(( char *ptr, char **outptr ));
extern char *pp_discard _AP(( char *ptr, char **outptr ));
extern char *pp_copy_nb _AP(( char *ptr, char **outptr ));
extern char *pp_copy_nt _AP(( char *ptr, char **outptr )); // no tags

/* Structure giving the "section_designators": those strings appearing */
/* at the beginning of a line which indicates a new section begins. */
/* The names of the section_designators must be sorted in reverse */
/* alphabetic order, and the last name must be the empty string */

typedef struct sectionstr {
    char *name;                 /* String that appears at beginning of a */
                                /* line indicating a new section starts */
                                /* (and the old one ends) */
    long  name_len;              /* Length of name that needs to match */
    char *(*parse)();           /* Procedure to handle this section. */
                                /* Normally 'copy' or 'discard' unless */
                                /* special attention is needed */
    char id;                    /* One char section designator (see */
                                /* the index_spec file for this collection) */
                                /* If id matches DISCARD_SECTION, then the */
                                /* section is not output for parsing. */
    long flags;                 /* Boolean flags describing the parsing */
                                /* of this section */
/* Boolean flag. If set, this entire  section is on the present line */
#define PP_ONELINE  (1)
/* Boolean flag. If set, this section begins a new document within this file */
#define PP_NEWDOC   (1 << 1)
/* Boolean flag. Set for exactly one section.  if TRUE, this section is
   the default section when no other rule applies. 
   OBSOLETE */
/* #define PP_DEFSECTION  (1 << 2) */
/* Boolean flag. If set, "name" should be part of the section included
   for display purposes. Note that "name" is never output for parsing.
   OBSOLETE */
#define PP_DISPINC   (1 << 3)
} PP_SECTIONS;

typedef struct {
    PP_SECTIONS *pp_sections;
    long num_pp_sections;
    PP_SECTIONS default_section;
    long doc_type;                 /* Preparser type, collection dependent.
                                      Possibly from values below */
    char *filter;                /* If non-NULL, a UNIX program to be run
                                    on every document text file */
} PP_INFO;

#define PP_TYPE_NEWS 1
#define PP_TYPE_EXPNEWS 2
#define PP_TYPE_MAIL 3
#define PP_TYPE_SMART 4
#define PP_TYPE_DOCSMART 5
#define PP_TYPE_FW 6
#define PP_TYPE_TEXT 7
#define PP_TYPE_TREC 8

#endif
