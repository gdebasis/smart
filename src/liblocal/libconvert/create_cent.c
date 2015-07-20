#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libinter/create_cent.c,v 11.2 1993/02/03 16:51:25 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Find the centroid vector of several vectors
 *2 create_cent(list, cent, inst)
 *3 EID_LIST *list;
 *3 VEC *cent;
 *3 int inst;
 *4 init_create_cent (SPEC *spec, (char *)prefix)
 *5   "<prefix>weight"

 *4 close_create_cent (inst)

 *7
***********************************************************************/


#include <ctype.h>
#include "common.h"
#include "context.h"
#include "functions.h"
#include "inst.h"
#include "local_eid.h"
#include "param.h"
#include "proc.h"
#include "retrieve.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "vector.h"

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("inter.centroid.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static char *pprefix;
static PROC_TAB *weight;	/* To re-weight centroid vector */
static SPEC_PARAM_PREFIX pspec_args[] = {
    {&pprefix, "weight",         getspec_func, (char *) &weight},
    };
static int num_pspec_args = sizeof( pspec_args ) / sizeof( pspec_args[0] );

typedef struct {
    /* bookkeeping */
    int valid_info;

    int did_vec_inst;
    PROC_TAB *weight;
    int weight_inst;
    int eid_inst;        /* for partlist uni-->multi conversion */

    int num_vecs;	/* Total number of vectors in the list */

    long *ctype_len[2];
    CON_WT *con_wtp[2];
    long num_ctype[2];
    long num_conwt[2];
    long max_num_ctype[2];
    long max_num_conwt[2];
    int current_buf;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static int add_vec _AP (( VEC *new_vec, STATIC_INFO *ip ));

int
init_create_cent (spec, prefix)
SPEC *spec;
char *prefix;
{
    STATIC_INFO *ip;
    int new_inst;
    long old_context;
    char tmp_prefix[PATH_LEN];

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return UNDEF;

    PRINT_TRACE (2, print_string, "Trace: entering init_create_cent");

    NEW_INST( new_inst );
    if (UNDEF == new_inst)
        return UNDEF;
    ip = &info[new_inst];

    ip->valid_info = 1;

    pprefix = prefix;
    if ( UNDEF == lookup_spec_prefix( spec, &pspec_args[0], num_pspec_args ) )
	return UNDEF;

    sprintf(tmp_prefix, "%scentroid.", (prefix == NULL) ? "" : prefix);
    if (UNDEF == (ip->eid_inst = init_eid_util(spec, tmp_prefix)))
	return UNDEF;

    old_context = get_context();
    sprintf(tmp_prefix, "%snormalize.", (prefix == NULL) ? "" : prefix);
    set_context (CTXT_DOC);
    ip->weight = weight;
    if (UNDEF == (ip->weight_inst = ip->weight->init_proc( spec, tmp_prefix )))
        return UNDEF;
    set_context (old_context);

    ip->max_num_conwt[0] = ip->max_num_conwt[1] = 0;
    ip->con_wtp[0] = ip->con_wtp[1] = NULL;
    ip->max_num_ctype[0] = ip->max_num_ctype[1] = 0;
    ip->ctype_len[0] = ip->ctype_len[1] = NULL;

    PRINT_TRACE (2, print_string, "Trace: leaving init_create_cent");
    return new_inst;
}

int
create_cent(list, cent, inst)
EID_LIST *list;
VEC *cent;
int inst;
{
    STATIC_INFO *ip;
    int i;
    VEC vec;

    ip  = &info[inst];
    ip->num_vecs = 0;
    ip->current_buf = 1;
    ip->num_conwt[0] = ip->num_conwt[1] = 0;
    ip->num_ctype[0] = ip->num_ctype[1] = 0;
    for ( i = 0; i < list->num_eids; i++ ) {
	if (UNDEF == eid_vec(list->eid[i], &vec, ip->eid_inst) ||
	    UNDEF == add_vec(&vec, ip))
	    return UNDEF;
    }
    cent->num_ctype = ip->num_ctype[ip->current_buf];
    cent->num_conwt = ip->num_conwt[ip->current_buf];
    cent->ctype_len = ip->ctype_len[ip->current_buf];
    cent->con_wtp = ip->con_wtp[ip->current_buf];

    /* Reweight the centroid vector, used for normalization */
    if ( UNDEF == ip->weight->proc( cent, (VEC *)NULL, ip->weight_inst ) )
	return UNDEF;

    return 1;
}

int
close_create_cent (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_create_cent");
    if (!VALID_INST(inst)) {
        set_error( SM_ILLPA_ERR, "Instantiation", "close_create_cent");
        return UNDEF;
    }
    ip = &info[inst];

    if (UNDEF == close_eid_util(ip->eid_inst) ||
	UNDEF == ip->weight->close_proc( ip->weight_inst ))
	return UNDEF;

    if ( ip->max_num_ctype[0] )
	Free(ip->ctype_len[0]);
    if ( ip->max_num_ctype[1] )
	Free(ip->ctype_len[1]);

    if ( ip->max_num_conwt[0] )
	Free(ip->con_wtp[0]);
    if ( ip->max_num_conwt[1] )
	Free(ip->con_wtp[1]);

    ip->valid_info = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving close_create_cent");
    return 0;
}

static int
add_vec (new_vec, ip)
VEC *new_vec;
STATIC_INFO *ip;
{
    long ctype;
    int next_buf = (ip->current_buf+1)%2;
    CON_WT *con_wtp1, *con_wtp2;
    CON_WT *end_ctype1, *end_ctype2;
    long num_con_wtp = 0;
    long num_ctype = 0;

    if ( ip->max_num_ctype[next_buf] <
	            ip->num_ctype[ip->current_buf] + new_vec->num_ctype ) {
	if ( ip->max_num_ctype[next_buf] )
	    Free(ip->ctype_len[next_buf]);
	ip->max_num_ctype[next_buf] = 
	    ip->num_ctype[ip->current_buf] + new_vec->num_ctype;
	if ( NULL== (ip->ctype_len[next_buf] =
		     Malloc(ip->max_num_ctype[next_buf], long)) )
	    return UNDEF;
    }

    if ( ip->max_num_conwt[next_buf] <
	            ip->num_conwt[ip->current_buf] + new_vec->num_conwt ) {
	if ( ip->max_num_conwt[next_buf] )
	    Free(ip->con_wtp[next_buf]);
	ip->max_num_conwt[next_buf] =
	    ip->num_conwt[ip->current_buf] + new_vec->num_conwt;
	if ( NULL== (ip->con_wtp[next_buf] =
		     Malloc(ip->max_num_conwt[next_buf], CON_WT)) )
	    return UNDEF;
    }

    con_wtp1 = ip->con_wtp[ip->current_buf];
    con_wtp2 = new_vec->con_wtp;

    for (ctype = 0;
	 ctype < ip->num_ctype[ip->current_buf] && ctype < new_vec->num_ctype; 
	 ctype++) {
        long last_con_wtp = num_con_wtp;

	end_ctype1 = &con_wtp1[ip->ctype_len[ip->current_buf][ctype]];
	end_ctype2 = &con_wtp2[new_vec->ctype_len[ctype]];

	while (con_wtp1 < end_ctype1 && con_wtp2 < end_ctype2) {
	    if (con_wtp1->con < con_wtp2->con) {
		ip->con_wtp[next_buf][num_con_wtp].con = con_wtp1->con;
		ip->con_wtp[next_buf][num_con_wtp++].wt = con_wtp1->wt;
		con_wtp1++;
	    }
	    else if (con_wtp1->con > con_wtp2->con) {
		ip->con_wtp[next_buf][num_con_wtp].con = con_wtp2->con;
		ip->con_wtp[next_buf][num_con_wtp++].wt = con_wtp2->wt;
		con_wtp2++;
	    }
	    else {
		ip->con_wtp[next_buf][num_con_wtp].con = con_wtp2->con;
		ip->con_wtp[next_buf][num_con_wtp++].wt =
		    con_wtp1->wt + con_wtp2->wt;
		con_wtp1++;
		con_wtp2++;
	    }
	}
	while ( con_wtp1 < end_ctype1 ) {
	    ip->con_wtp[next_buf][num_con_wtp].con = con_wtp1->con;
	    ip->con_wtp[next_buf][num_con_wtp++].wt = con_wtp1->wt;
	    con_wtp1++;
	}
	while ( con_wtp2 < end_ctype2 ) {
	    ip->con_wtp[next_buf][num_con_wtp].con = con_wtp2->con;
	    ip->con_wtp[next_buf][num_con_wtp++].wt = con_wtp2->wt;
	    con_wtp2++;
	}
	ip->ctype_len[next_buf][num_ctype++] = num_con_wtp - last_con_wtp;
    }

    while ( num_ctype < ip->num_ctype[ip->current_buf] ) {
	end_ctype1 = &con_wtp1[ip->ctype_len[ip->current_buf][num_ctype]];
	while ( con_wtp1 < end_ctype1 ) {
	    ip->con_wtp[next_buf][num_con_wtp].con = con_wtp1->con;
	    ip->con_wtp[next_buf][num_con_wtp++].wt = con_wtp1->wt;
	    con_wtp1++;
	}
	ip->ctype_len[next_buf][num_ctype] =
	    ip->ctype_len[ip->current_buf][num_ctype];
        num_ctype++;
    }

    while ( num_ctype < new_vec->num_ctype ) {
	end_ctype2 = &con_wtp2[new_vec->ctype_len[num_ctype]];
	while ( con_wtp2 < end_ctype2 ) {
	    ip->con_wtp[next_buf][num_con_wtp].con = con_wtp2->con;
	    ip->con_wtp[next_buf][num_con_wtp++].wt = con_wtp2->wt;
	    con_wtp2++;
	}
	ip->ctype_len[next_buf][num_ctype] = new_vec->ctype_len[num_ctype];
        num_ctype++;
    }
    ip->num_ctype[next_buf] = num_ctype;
    ip->num_conwt[next_buf] = num_con_wtp;
    ip->current_buf = next_buf;
    ip->num_vecs++;

    return 0;
}




