#ifndef DOCDESCH
#define DOCDESCH
/*  $Header: /home/smart/release/src/h/docdesc.h,v 11.2 1993/02/03 16:47:27 smart Exp $ */

#include "proc.h"

/* DOC_DESC, SECTION_INFO, CTYPE_INFO, CTYPE_MAP give instructions to the
   parser (and other routines) about what the pre-parsed doc looks like,
   and what should be done about the text in each section of the 
   pre-parsed doc. */
typedef struct {
    char *name;
    PROC_TAB *con_to_token;        /* Procedure that maps concepts to tokens */
    PROC_TAB *weight_ctype;        /* Procedure to weight this ctype */ 
    PROC_TAB *store_aux;           /* Procedure to store this ctype
                                      subvector (eg inverted file) */
    PROC_TAB *inv_sim;             /* Procedure to retrieve on this subvector
                                      of query using inverted approach */
    PROC_TAB *seq_sim;             /* Procedure to retrieve on this subvector
                                      of query using sequential approach */
} CTYPE_INFO;

typedef struct {
    char *name;           /* name of section (ONLY FIRST CHAR SIGNIFICANT */
    PROC_TAB *tokenizer;  /* procedure to be called to tokenize this section */
    PROC_TAB *parser;     /* procedure to be called to parse this section */
} SECTION_INFO;

typedef struct {
    long num_sections;
    long num_ctypes;
    CTYPE_INFO *ctypes;     /* ctype dependent procedures to be called */
    SECTION_INFO *sections; /* section dependent procs and info */
} SM_INDEX_DOCDESC;

#endif /* DOCDESCH */
