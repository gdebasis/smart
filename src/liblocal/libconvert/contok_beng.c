#ifdef RCSID
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Get token (ISCII chars) and output transliterated (ASCII) token
 *1 local.convert.tup.contok_beng

 *7 Get token corresponding to concept no. using standard con_to_token
 *7 method. This is a token consisting of ISCII characters. Convert this
 *7 to an ASCII token by transliteration and return the transliterated
 *7 token.
 *9 WARNING: a set of 4 character buffers are used cyclically to store
 *9  the transliterated token. Repeated calls may therefore result in
 *9  overwriting tokens returned earlier.
***********************************************************************/

#include <ctype.h>
#include "common.h"
#include "context.h"
#include "functions.h"
#include "inst.h"
#include "inv.h"
#include "io.h"
#include "param.h"
#include "proc.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "vector.h"
#include "8bit-iscii.h"

#define NUM_BUFS 4

static PROC_TAB *contok;
static SPEC_PARAM spec_args[] = {
    {"contok_beng.con_to_token", getspec_func, (char *)&contok},
    TRACE_PARAM ("contok_beng.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


/* Static info to be kept for each instantiation of this proc */
typedef struct {
    int valid_info;
    int contok_inst;
} STATIC_INFO;
static STATIC_INFO *info;
static int max_inst = 0;

static char tok_buf[NUM_BUFS][128];
static void translit();
static int char_type();

int
init_contok_beng (spec, prefix)
SPEC *spec;
char *prefix;
{
    STATIC_INFO *ip;
    int new_inst;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);
    PRINT_TRACE (2, print_string, "Trace: entering init_contok_beng");

    NEW_INST (new_inst);
    if (UNDEF == new_inst) return (UNDEF);
    ip = &info[new_inst];

    if (UNDEF == (ip->contok_inst = contok->init_proc(spec, prefix)))
	return (UNDEF);
    ip->valid_info++;

    PRINT_TRACE (2, print_string, "Trace: leaving init_contok_beng");
    return (0);
}

int
contok_beng (con, word, inst)
long *con;
char **word;
int inst;
{
    char *beng_tok;
    static int which_buf;
    int status;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering contok_beng");
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "contok_beng");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (UNDEF == (status = contok->proc(con, &beng_tok, ip->contok_inst)))
	return(UNDEF);
    /* Transliterate */
    translit(beng_tok, which_buf);
    *word = tok_buf[which_buf];
    which_buf = (which_buf + 1) % NUM_BUFS;

    PRINT_TRACE (2, print_string, "Trace: leaving contok_beng");
    return (status);
}

int
close_contok_beng(inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_contok_beng");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_contok_beng");
        return (UNDEF);
    }
    ip  = &info[inst];   

    if (UNDEF == contok->close_proc(ip->contok_inst))
	return (UNDEF);
    ip->valid_info--;

    PRINT_TRACE (2, print_string, "Trace: entering close_contok_beng");
    return (0);
}


static void
translit(bangla, bufno)
char *bangla;
int bufno;
{
    char *c = tok_buf[bufno], *tmp;
    unsigned char *uc;
    int prevtype = -1, currtype;

    for (uc = bangla; *uc; uc++) {
        if (isascii(*uc)) {
           *c++ = *uc;
           prevtype = -1; 
	   continue;
	} 
        if (D == *uc || Dh == *uc) {
	    if (R == *(uc+1)) /* phutki */
		continue;
        }
        currtype = char_type(*uc);
        if (CONSONANT == prevtype &&
            /* VOWEL != currtype && - omit (as suggested by Arindam) */
	    MATRA != currtype &&
            HOSH != currtype)
	    *c++ = 'a';
        /* special handling for ontostho-jo-phola */
        if (J == *uc && HOSH == prevtype)
	    *c++ = 'y';
        else { 
	    tmp = equivalent[*uc - ISCII_OFFSET];
	    while (*c++ = *tmp++);
	    c--; /* don't want terminal '\0' from equivalent right now */
	}
	prevtype = currtype;
    }
    if (CONSONANT == prevtype)
	*c++ = 'a';
    *c = '\0';
    return;
}

static int 
char_type(c)
int c;
{
    if (c == y) return YO; /* ontostho a */
    if (c == hosh) return HOSH;
    if ((c >= k && c <= r) ||
        (c >= l && c <= h) ||
	c == R)
       return CONSONANT;
    if (c >= a && c <= ou) return VOWEL;
    if ((c >= AA && c <= RI) ||
        (c >= E && c <= OU))
       return MATRA;
    if (c < a) return AJOG;
    return OTHER;
}
