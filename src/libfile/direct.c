#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfile/direct.c,v 11.0 1992/07/21 18:20:51 chrisb Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "rel_header.h"
#include "direct.h"
#include "buf.h"

_SDIRECT _Sdirect[MAX_NUM_DIRECT];

SM_BUF write_direct_buf[MAX_NUM_DIRECT];
