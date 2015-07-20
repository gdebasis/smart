#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libinter/compare_util_doc.c,v 11.2 1993/02/03 16:52:09 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Various procedures used by compare, segment, theme, path programs.

***********************************************************************/

#include <ctype.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include "common.h"
#include "compare.h"
#include "context.h"
#include "local_eid.h"
#include "functions.h"
#include "inst.h"
#include "inter.h"
#include "param.h"
#include "proc.h"
#include "retrieve.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "vector.h"



/********************   PROCEDURE DESCRIPTION   ************************
 *0 Build a list of unique eids from the result tuples.
***********************************************************************/
int
build_eidlist(altres, elist, max_eids)
ALT_RESULTS *altres;
EID_LIST *elist;
int *max_eids;
{
    int i, j;
    RESULT_TUPLE *rtup;

    PRINT_TRACE (2, print_string, "Trace: entering build_eidlist");    
    if (*max_eids < 2 * altres->num_results) {
	if (*max_eids) Free(elist->eid);
	*max_eids += 2 * altres->num_results;
	if (NULL == (elist->eid = Malloc(*max_eids, EID)))
	    return(UNDEF);
    }

    for (i = 0, rtup = altres->results; 
	 rtup < altres->results + altres->num_results; rtup++) { 
	elist->eid[i++] = rtup->qid;
	elist->eid[i++] = rtup->did;
    }
    elist->num_eids = i;
    qsort((char *)elist->eid, i, sizeof(EID), compare_eids);
    for (j=0, i=1; i < elist->num_eids; i++)
	if (!EID_EQUAL(elist->eid[j], elist->eid[i]))
	    elist->eid[++j] = elist->eid[i];
    elist->num_eids = (j > 0) ? j + 1 : 0;

    PRINT_TRACE (2, print_string, "Trace: leaving build_eidlist");
    return(1);
}


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Sort RESULT_TUPLES in descending order of similarity.
***********************************************************************/
int
compare_rtups_sim(r1, r2)
RESULT_TUPLE *r1;
RESULT_TUPLE *r2;
{
    if (r1->sim < r2->sim)	/* descending order */
	return 1;
    if (r1->sim > r2->sim)
	return -1;
    return 0;
}


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Compare 2 EIDs
***********************************************************************/
int
eid_greater(e1, e2)
EID *e1;
EID *e2;
{
    if (e1->ext.type == QUERY && e2->ext.type != QUERY) return(0);
    if (e1->ext.type != QUERY && e2->ext.type == QUERY) return(1);

    if (e1->id > e2->id) return 1;
    if (e1->id < e2->id) return 0;
    
    if (e1->ext.type < e2->ext.type) return 1;
    if (e1->ext.type > e2->ext.type) return 0;
    
    if (e1->ext.num > e2->ext.num) return 1;

    return 0;
}


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Sort EIDS in ascending order.
***********************************************************************/
int
compare_eids(e1, e2)
EID *e1;
EID *e2;
{
    if (eid_greater(e1, e2)) return 1;
    if (EID_EQUAL((*e1), (*e2))) return(0);
    return -1;
}


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Sort RESULT_TUPLES in ascending order of qid, did.
***********************************************************************/
int
compare_rtups(r1, r2)
RESULT_TUPLE *r1;
RESULT_TUPLE *r2;
{
    if (eid_greater(&r1->qid, &r2->qid)) return 1;
    if (EID_EQUAL(r1->qid, r2->qid)) {
	if (eid_greater(&r1->did, &r2->did)) return 1;
	if (EID_EQUAL(r1->did, r2->did)) return 0;
    }
    return -1;
}
