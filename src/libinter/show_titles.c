#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libinter/show_titles.c,v 11.2 1993/02/03 16:51:31 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "textloc.h"
#include "io.h"
#include "smart_error.h"
#include "spec.h"
#include "proc.h"
#include "tr_vec.h"
#include "context.h"
#include "retrieve.h"
#include "docindex.h"
#include "inter.h"
#include "inst.h"
#include "trace.h"

static long docid_width;
static SPEC_PARAM spec_args[] = {
    {"inter.docid_width",  getspec_long, (char *) &docid_width},
    TRACE_PARAM ("inter.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

/* Format of individual title line */
#define DID_WIDTH (ip->docid_width)
#define ACT_OFFSET (DID_WIDTH+2)
#define REL_OFFSET (DID_WIDTH+3)
#define RANK_OFFSET (DID_WIDTH+6)
#define SIM_OFFSET (DID_WIDTH+11)
#define TITLE_OFFSET (DID_WIDTH+16)
#define DISPLAY_WIDTH   (79)
#define TITLE_WIDTH (DISPLAY_WIDTH-TITLE_OFFSET)

typedef struct {
    SM_BUF menu_buf;
    long *entry_offset;       /* offset into menu_buf of ith entries title */
    long num_entries;
} MENU;

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;
    SPEC *saved_spec;

    long docid_width;
    int util_inst;

    /* current titles */
    MENU title_menu;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;


static int compare_tr_rank();

int
init_show_titles (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;
    long i;

    /* Check to see if this spec file is already initialized (only need
       one initialization per spec file) */
    for (i = 0; i <  max_inst; i++) {
        if (info[i].valid_info && spec == info[i].saved_spec) {
            info[i].valid_info++;
            return (i);
        }
    }

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_show_titles");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
       return (UNDEF);
    
    ip = &info[new_inst];
    ip->saved_spec = spec;

    /* WARNING.  docid_width should really get set using is->orig_spec so */
    /* it doesn't change upon an Euse command */
    ip->docid_width = docid_width;

    ip->title_menu.menu_buf.size = 0;
    ip->title_menu.num_entries = 0;
    ip->title_menu.entry_offset = (long *) NULL;
    if (UNDEF == (ip->util_inst = init_inter_util (spec, (char *) NULL)))
        return (UNDEF);

    ip->valid_info = 1;;

    PRINT_TRACE (2, print_string, "Trace: leaving init_show_titles");

    return (new_inst);
}

int
close_show_titles (inst)
int inst;
{
    STATIC_INFO *ip;
    PRINT_TRACE (2, print_string, "Trace: entering close_show_titles");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_show_titles");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Check to see if still have valid instantiations using this data */
    if (--ip->valid_info > 0)
        return (0);

    if (UNDEF == close_inter_util (ip->util_inst))
        return (UNDEF);

    if (ip->title_menu.entry_offset != (long *) NULL)
        (void) free ((char *) ip->title_menu.entry_offset);
    if (ip->title_menu.menu_buf.size > 0)
        (void) free (ip->title_menu.menu_buf.buf);

    PRINT_TRACE (2, print_string, "Trace: leaving close_show_titles");

    return (0);
}


int
inter_prepare_titles (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    long i;
    int feedback_iter;
    char temp_buf[PATH_LEN];
    TR_VEC *tr_vec = is->retrieval.tr;
    char *title;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering inter_prepare_titles");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.prepare_titles");
        return (UNDEF);
    }
    ip  = &info[inst];

    ip->title_menu.menu_buf.end = 0;
    (void) sprintf( temp_buf, "%*sAction Rank Sim Title\n", (int)-(DID_WIDTH-1),
		   "Num" );
    if (UNDEF == add_buf_string (temp_buf, &ip->title_menu.menu_buf))
        return (UNDEF);
    if (tr_vec->num_tr == 0) {
        if (UNDEF == add_buf_string ("No docs retrieved\n",
                                     &ip->title_menu.menu_buf))
            return (UNDEF);
        return (0);
    }

    /* Reserve space for pointers into the full title display */
    if (tr_vec->num_tr+1 > ip->title_menu.num_entries) {
        if (0 == ip->title_menu.num_entries) {
            if (NULL == (ip->title_menu.entry_offset = (long *) malloc
                         ((unsigned) (tr_vec->num_tr+1) * sizeof (long))))
                return (UNDEF);
        }
        else {
            if (NULL == (ip->title_menu.entry_offset = (long *)
                         realloc((char *) ip->title_menu.entry_offset,
                                 (unsigned)(tr_vec->num_tr+1)*sizeof (long))))
                return (UNDEF);
        }
        ip->title_menu.num_entries = tr_vec->num_tr + 1;
    }

    /* Sort incoming docs in decreasing similarity */
    qsort ((char *) tr_vec->tr,
           (int) tr_vec->num_tr,
           sizeof (TR_TUP), compare_tr_rank);

    feedback_iter = tr_vec->tr[0].iter;

    /* Add each doc's title to title menu */
    for (i = 0; i < tr_vec->num_tr; i++) {
        if (feedback_iter != tr_vec->tr[i].iter) {
            (void) sprintf (temp_buf,
                            "\nPreviously retrieved docs from iteration %d\n",
                            (int) tr_vec->tr[i].iter);
            if (UNDEF == add_buf_string (temp_buf, &ip->title_menu.menu_buf))
                return (UNDEF);
            feedback_iter = tr_vec->tr[i].iter;
        }

        if (UNDEF == inter_get_title (is, tr_vec->tr[i].did,
                                      &title, ip->util_inst))
            return (UNDEF);

        (void) sprintf (temp_buf,
                        "%*ld     %4ld %4.2f %.*s\n",
                        (int) -DID_WIDTH, tr_vec->tr[i].did,
			i+1,
                        tr_vec->tr[i].sim,
                        (int) TITLE_WIDTH, 
                        title);
        ip->title_menu.entry_offset[i] = ip->title_menu.menu_buf.end;
        if (UNDEF == add_buf_string (temp_buf, &ip->title_menu.menu_buf))
            return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving inter_prepare_titles");

    return (1);
}


int
inter_show_titles (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    register long i;
    TR_VEC *tr_vec = is->retrieval.tr;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering inter_show_titles");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.show_titles");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (tr_vec->num_tr == 0 || ip->title_menu.entry_offset == NULL) {
        if (UNDEF == add_buf_string ("No documents to display", &is->err_buf))
            return (UNDEF);
        return (0);
    }

    /* Prepare action and rel judgement fields for display */
    for (i = 0; i < tr_vec->num_tr; i++) {
        ip->title_menu.menu_buf.buf[ip->title_menu.entry_offset[i] +ACT_OFFSET]
            = tr_vec->tr[i].action ?
                tr_vec->tr[i].action : ' ';
	if (tr_vec->tr[i].rel==1)
	    ip->title_menu.menu_buf.buf[ip->title_menu.entry_offset[i] +REL_OFFSET]
		= 'Y';
	else if (tr_vec->tr[i].rel==UNDEF)
	    ip->title_menu.menu_buf.buf[ip->title_menu.entry_offset[i] +REL_OFFSET]
		= 'P';
	else if (tr_vec->tr[i].rel==0 && tr_vec->tr[i].action)
	    ip->title_menu.menu_buf.buf[ip->title_menu.entry_offset[i] +REL_OFFSET]
		= 'N';
    }

    if (UNDEF == add_buf (&ip->title_menu.menu_buf, &is->output_buf))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving inter_show_titles");

    return (1);
}

static int
compare_tr_rank (tr1, tr2)
register TR_TUP *tr1;
register TR_TUP *tr2;
{
    if (tr1->iter != tr2->iter) {
        if (tr1->iter > tr2->iter)
            return (-1);
        return (1);
    }
    if (tr1->rank < tr2->rank) {
        return (-1);
    }
    if (tr1->rank > tr2->rank) {
        return (1);
    }
    return (0);
}

