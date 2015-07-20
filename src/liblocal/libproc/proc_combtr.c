#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libproc/proc_combtr.c,v 11.2 1993/02/03 16:52:20 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "proc.h"

extern int init_combtr(), combtr(), close_combtr();
PROC_TAB proc_combtr[] = {
    {"combtr", init_combtr, combtr, close_combtr,},
  };
int num_proc_combtr = sizeof (proc_combtr) / sizeof (proc_combtr[0]);

TAB_PROC_TAB lproc_combtr[] = {
    {"combtr",	TPT_PROC,	NULL,	proc_combtr,	&num_proc_combtr,},
  };
int num_lproc_combtr = sizeof (lproc_combtr) / sizeof (lproc_combtr[0]);
