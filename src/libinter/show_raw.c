#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libinter/show_raw.c,v 11.2 1993/02/03 16:51:30 smart Exp $";
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

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    SPEC *saved_spec;
    int util_inst;
    long docid_width;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;


int 
init_show_raw (spec, unused)
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

    PRINT_TRACE (2, print_string, "Trace: entering init_show_raw");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];

    ip->saved_spec = spec;
    ip->docid_width = docid_width;
    if (UNDEF == (ip->util_inst = init_inter_util (spec, (char *) NULL)))
        return (UNDEF);

    ip->valid_info++;

    PRINT_TRACE (2, print_string, "Trace: leaving init_show_raw");

    return (new_inst);
}

int
close_show_raw (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_show_raw");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.raw");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Check to see if still have valid instantiations using this data */
    if (--ip->valid_info > 0)
        return (0);
    
    if (UNDEF == close_inter_util (ip->util_inst))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_show_raw");

    return (1);
}

int
show_raw_tr (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    char temp_buf[PATH_LEN];
    TR_TUP *tr_tup;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering show_raw_tr");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.show_raw_tr");
        return (UNDEF);
    }
    ip  = &info[inst];

    
    (void) sprintf( temp_buf,
		   "%*s Rank\t%*s Similarity\tUseful?\tAction\tIter\n",
		   (int) -ip->docid_width, "Qid",
		   (int) -ip->docid_width, "Did" );
    if (UNDEF == add_buf_string( temp_buf, &is->output_buf))
        return (UNDEF);

    for (tr_tup = is->retrieval.tr->tr;
         tr_tup < &is->retrieval.tr->tr[is->retrieval.tr->num_tr];
         tr_tup++) {

        (void) sprintf (temp_buf,
                        "%*ld %ld\t%*ld %9.4f\t%c\t%c\t%d\n",
			(int) -ip->docid_width, is->retrieval.tr->qid,
                        tr_tup->rank,
			(int) -ip->docid_width, tr_tup->did,
                        tr_tup->sim,
                        (tr_tup->rel ? 'Y' : ' '),
                        (tr_tup->action ? tr_tup->action : ' '),
                        tr_tup->iter);
        if (UNDEF == add_buf_string (temp_buf, &is->output_buf))
            return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving show_raw_tr");

    return (1);
}

int
show_spec (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering show_spec");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.raw");
        return (UNDEF);
    }
    ip  = &info[inst];

    print_spec (ip->saved_spec, &is->output_buf);

    PRINT_TRACE (2, print_string, "Trace: leaving show_spec");

    return (1);
}

int
show_raw_textloc (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    TEXTLOC textloc;
    char temp_buf[PATH_LEN];
    TR_TUP *tr_tup;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering show_raw_textloc");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "inter.raw_textloc");
        return (UNDEF);
    }
    ip  = &info[inst];

    for (tr_tup = is->retrieval.tr->tr;
         tr_tup < &is->retrieval.tr->tr[is->retrieval.tr->num_tr];
         tr_tup++) {
        if (1 != inter_get_textloc (is, tr_tup->did, &textloc, ip->util_inst)){
            (void) sprintf (temp_buf,
                            "/nDocument %ld Not Available\n", tr_tup->did);
            if (UNDEF == add_buf_string (temp_buf, &is->output_buf))
                return (UNDEF);
        }
        else {
            print_int_textloc (&textloc, &is->output_buf);
        }
    }

    PRINT_TRACE (2, print_string, "Trace: leaving show_raw_textloc");

    return (1);
}
