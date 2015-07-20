#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libproc/proc.c,v 11.2 1993/02/03 16:52:22 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Root of local hierarchy table giving other tables of procedures
 *1 local
 *7 Procedure table mapping a string procedure table name to table addresses.
 *8 Current possibilities are "none", 
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "proc.h"
#include "spec.h"

extern TAB_PROC_TAB lproc_index[];
extern int num_lproc_index;
extern TAB_PROC_TAB lproc_retrieve[];
extern int num_lproc_retrieve;
extern TAB_PROC_TAB lproc_combtr[];
extern int num_lproc_combtr;
extern TAB_PROC_TAB lproc_print[]; 
extern int num_lproc_print;
extern TAB_PROC_TAB lproc_fdbk[];
extern int num_lproc_fdbk;
/* extern PROC_TAB lproc_inter[]; */
/* extern int num_lproc_inter; */
extern TAB_PROC_TAB lproc_convert[];
extern int num_lproc_convert;
extern TAB_PROC_TAB lproc_ocr[];
extern int num_lproc_ocr;
/*
extern TAB_PROC_TAB lproc_infoneed[];
extern int num_lproc_infoneed;
*/

TAB_PROC_TAB lproc_root[] = {
    {"index",	TPT_TAB,	lproc_index,	NULL,	&num_lproc_index},
    {"retrieve",TPT_TAB,	lproc_retrieve,	NULL,	&num_lproc_retrieve},
    {"combtr",	TPT_TAB,	lproc_combtr,	NULL,	&num_lproc_combtr,},
    {"print",	TPT_TAB,	lproc_print,	NULL,	&num_lproc_print,},
    {"feedback",TPT_TAB,	lproc_fdbk,     NULL,	&num_lproc_fdbk,},
/*   {"inter",	TPT_PROC,	NULL,	lproc_inter,	&num_lproc_inter}, */
    {"convert",	TPT_TAB,	lproc_convert,	NULL,	&num_lproc_convert},
    {"ocr",	TPT_TAB,	lproc_ocr,	NULL,	&num_lproc_ocr},
//    {"infoneed", TPT_TAB,	lproc_infoneed,	NULL,	&num_lproc_infoneed},
    };

int num_lproc_root = sizeof (lproc_root) / sizeof (lproc_root[0]);
