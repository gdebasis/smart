#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libconvert/tr_tr_interleave.c,v 11.2 1993/02/03 16:51:43 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.

   For patents, we break up a single query patent into component fields.
   Here, we take as input the the tr file resulting from retrieving using
   each component and merge the results. For a start, we simply do
   1-way interleaving of the results. 
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Take a tr file and apply a filter to get a new tr file.
 *1 local.convert.obj.tr_tr
 *2 tr_tr_interleave (in_tr_file, out_tr_file, inst)
 *3 char *in_tr_file;
 *3 char *out_tr_file;
 *3 int inst;
 *4 init_tr_tr_interleave (spec, unused)
 *5   "tr_filter.in_tr_file"
 *5   "tr_filter.in_tr_file.rmode"
 *5   "tr_filter.out_tr_file"
 *5   "tr_filter.trace"
 *4 close_tr_tr_interleave (inst)
 *7 
***********************************************************************/

#include <ctype.h>
#include <search.h>
#include "common.h"
#include "param.h"
#include "dict.h"
#include "functions.h"
#include "io.h"
#include "proc.h"
#include "smart_error.h"
#include "spec.h"
#include "tr_vec.h"
#include "trace.h"
#include "vector.h"
#include "textloc.h"

static long tr_mode;
static long top;
static PROC_TAB *vec_vec;
static float coeff;
static char *textloc_file;
static long textloc_mode;
static int  textloc_index;
static long  interleaveNum;
static int   numFuseRes;
static char* fusionMethName;

static SPEC_PARAM spec_args[] = {
    {"tr_fuse.textloc_file",     getspec_dbfile, (char *) &textloc_file},
    {"tr_fuse.textloc_file.rmode",getspec_filemode, (char *) &textloc_mode},
    {"tr_fuse.in_tr_file.rmode", getspec_filemode,	(char *) &tr_mode},
    {"tr_fuse.top_wanted",	getspec_long,	(char *) &top},
    {"tr_fuse.fusion_method", getspec_string, (char *) &fusionMethName},
    //{"tr_fuse.interleave_number",	getspec_long,	(char *) &interleaveNum},
    TRACE_PARAM ("tr_fuse.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int did_vec_inst, qid_vec_inst, vv_inst;
static int comp_did(TR_TUP*, TR_TUP*);
static int comp_sim(TR_TUP*, TR_TUP*);
static int comp_combmnz(TR_TUP*, TR_TUP*);
static int comp_combanz(TR_TUP*, TR_TUP*);
static int get_rank(const void *x);
static TR_VEC* tr_vec_buff;
static int num_tr_vec_buff;
static TR_TUP* fused_tr_tup_buff, *this_trtup;

static void storeSim(const void *nodep, const VISIT which, const int depth);
static int fuseResultsByCombSum(char* thisfile, int num_segs, TR_VEC* fused_tr_vec);
static int fuseResultsByInterleaving(char* thisfile, int num_segs, TR_VEC* fused_tr_vec);
static int fuseResultsByKWayInterleaving(char* thisfile, int num_segs, TR_VEC* fused_tr_vec);
static int fuseResultsByNSimMerging(char* thisfile, int num_segs, TR_VEC* fused_tr_vec);

typedef int (*fuseP) (char* thisfile, int num_segs, TR_VEC* fused_tr_vec);       
static fuseP getFusionFunction()
{
	if (0) { }
	else if (!strcmp(fusionMethName, "interleave")) {
		return fuseResultsByInterleaving;
	}
	else if (!strcmp(fusionMethName, "combsum")) {
		return fuseResultsByCombSum;
	}
	else {
		return NULL;
	}
}

static fuseP fusionMethod;

int
init_tr_tr_interleave (spec, unused)
SPEC *spec;
char *unused;
{
	int i;
    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_tr_tr_interleave");
    if (UNDEF == (textloc_index = open_textloc (textloc_file, textloc_mode)))
        return (UNDEF);

	num_tr_vec_buff = 500;
	if (NULL == (tr_vec_buff = Malloc(num_tr_vec_buff, TR_VEC)))
		return UNDEF;

	if (NULL == (fused_tr_tup_buff = Malloc(top, TR_TUP))) {
		return UNDEF;
	}

	fusionMethod = getFusionFunction();
    PRINT_TRACE (2, print_string, "Trace: leaving init_exp_rel_doc");

    return (0);
}

int
tr_tr_interleave (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
    int in_fd, out_fd, num_segs;
    float newsim;
    TR_VEC tr_vec, fused_tr_vec;
    TEXTLOC textloc;
	char *thisfile;
	static char prevfile[256];

    PRINT_TRACE (2, print_string, "Trace: entering tr_tr_interleave");

    /* Open input and output files */
    if (in_file == (char *) NULL) return UNDEF;
    if (out_file == (char *) NULL) return UNDEF;
    if (UNDEF == (in_fd = open_tr_vec (in_file, tr_mode)) ||
        UNDEF == (out_fd = open_tr_vec (out_file, SRDWR | SCREATE)))
      return (UNDEF);

	num_segs = 0;
	/* Read each tr_vec record in turn */
    while (1 == read_tr_vec (in_fd, &tr_vec)) {
		if (tr_vec.qid < global_start) continue;
		if (tr_vec.qid > global_end) break;

		if (NULL == (this_trtup = Malloc(tr_vec.num_tr, TR_TUP)))
			return UNDEF;
		memcpy(this_trtup, tr_vec.tr, tr_vec.num_tr * sizeof(TR_TUP)); 
		countSort(this_trtup, tr_vec.num_tr, sizeof(TR_TUP), get_rank, tr_vec.num_tr);

        textloc.id_num = tr_vec.qid;
        if (UNDEF == seek_textloc (textloc_index, &textloc) ||
			UNDEF == read_textloc (textloc_index, &textloc))
            return (UNDEF);

		thisfile = strstr(textloc.file_name, "PAC-");
		if (thisfile == NULL)
			return UNDEF;	// error in the file names... warning dependent on the file names
		if (*prevfile == 0) {
			snprintf(prevfile, sizeof(prevfile), "%s", thisfile);
		}
		else {
			if (strcmp(thisfile, prevfile)) {
				// The file name changes... which means that it's time
				// to read a new query.
				if (UNDEF == (*fusionMethod)(prevfile, num_segs, &fused_tr_vec))
					return UNDEF;

				// Save the fused results
				if (UNDEF == seek_tr_vec (out_fd, &fused_tr_vec) ||
				    UNDEF == write_tr_vec (out_fd, &fused_tr_vec))
				    return (UNDEF);

				snprintf(prevfile, sizeof(prevfile), "%s", thisfile);
				num_segs = 0;
			}
		}

		if (num_segs >= num_tr_vec_buff-1) {
			num_tr_vec_buff = num_segs<<1; 
			if (NULL == (tr_vec_buff = Realloc(tr_vec_buff, num_tr_vec_buff, TR_VEC)))
				return UNDEF;
		}

		tr_vec_buff[num_segs].qid = tr_vec.qid;
		tr_vec_buff[num_segs].num_tr = tr_vec.num_tr;
		tr_vec_buff[num_segs].tr = this_trtup;

		num_segs++;
    }

	// Fuse the last chunk, where there is no transition
	if (UNDEF == (*fusionMethod)(thisfile, num_segs, &fused_tr_vec))
		return UNDEF;
	if (UNDEF == seek_tr_vec (out_fd, &fused_tr_vec) ||
	    UNDEF == write_tr_vec (out_fd, &fused_tr_vec))
	    return (UNDEF);

    if(UNDEF == close_tr_vec (in_fd) ||
       UNDEF == close_tr_vec (out_fd))
      return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving tr_tr_interleave");
    return (1);
}

int fuseResultsByInterleaving(char* thisfile, int num_segs, TR_VEC* fused_tr_vec)
{
	int i = 0, j = 0, k = 0, atleastOneAdded = 0;
	static char key[32];
	ENTRY e, *ep;

	if (num_segs <= 0) return 1;

	// Hash table for detecting duplicate documents
	hcreate(top<<1);

	fused_tr_vec->qid = tr_vec_buff[0].qid;

	while (k < top) {
		atleastOneAdded = 0;
		for (i = 0; i < num_segs; i++) {
			if (k >= top)
				break;
			if (j >= tr_vec_buff[i].num_tr)
				continue;

			snprintf(key, sizeof(key), "%ld", tr_vec_buff[i].tr[j].did);
			e.key = key;
			if (NULL == (ep = hsearch(e, FIND))) {
				if (NULL == (ep = hsearch(e, ENTER)))
					return UNDEF;
			}
			else
				continue;	// no duplicate entries...

			atleastOneAdded = 1;
			memcpy(&fused_tr_tup_buff[k], &(tr_vec_buff[i].tr[j]), sizeof(TR_TUP));
			fused_tr_tup_buff[k].rank = k+1; 
			fused_tr_tup_buff[k].sim = top-k; // pseudo-sim values
			k++;
		}
		j++;
		if (!atleastOneAdded)
			break;
	}

	hdestroy();

	fused_tr_vec->tr = fused_tr_tup_buff;
	fused_tr_vec->num_tr = k;

	// Free the tr_tups of tr_vec_buffs
	for (i = 0; i < num_segs; i++) {
		Free(tr_vec_buff[i].tr);
	}
	return 1;
}

int fuseResultsByKWayInterleaving(char* thisfile, int num_segs, TR_VEC* fused_tr_vec)
{
	int i = 0, j = 0, k = 0;
	static char key[32];
	ENTRY e, *ep;
	int intl;

	// Hash table for detecting duplicate documents
	hcreate(top);

	fused_tr_vec->qid = tr_vec_buff[0].qid;

	while (k < top) {
		for (i = 0; i < num_segs; i++) {
			if (k >= top)
				break;
			if (j+interleaveNum >= tr_vec_buff[i].num_tr)
				continue;

			for (intl = 0; intl < interleaveNum; intl++) {
				if (k >= top)
					break;

				snprintf(key, sizeof(key), "%ld", tr_vec_buff[i].tr[j+intl].did);
				e.key = key;
				if (NULL == (ep = hsearch(e, FIND))) {
					if (NULL == (ep = hsearch(e, ENTER)))
						return UNDEF;
				}
				else
					continue;	// no duplicate entries...

				fused_tr_tup_buff[k] = tr_vec_buff[i].tr[j+intl];
				fused_tr_tup_buff[k].rank = k+1; 
				fused_tr_tup_buff[k].sim = top-k; // pseudo-sim values
				k++;
			}
		}
		j += intl;
	}

	hdestroy();

	fused_tr_vec->tr = fused_tr_tup_buff;
	fused_tr_vec->num_tr = k;

	// Free the tr_tups of tr_vec_buffs
	for (i = 0; i < num_segs; i++) {
		Free(tr_vec_buff[i].tr);
	}
	return 1;
}

void storeSim(const void *nodep, const VISIT which, const int depth)
{
	TR_TUP trtup;

    switch (which) {
        case preorder:
           break;
		case endorder:
		   break;
        case postorder:
		case leaf:
		   if (numFuseRes < top) {
	           trtup = *(*(TR_TUP**)nodep);
			   fused_tr_tup_buff[numFuseRes++] = trtup;
		   }
	}
}

int fuseResultsByCombSum(char* thisfile, int num_segs, TR_VEC* fused_tr_vec)
{
	int i = 0, j = 0, k = 0;
	static char key[32];
	float combsum;
	TR_TUP*  scoreTree = NULL;
	TR_TUP** node;	

	fused_tr_vec->qid = tr_vec_buff[0].qid;

	// Walk thru each document in each list and update the combsums
	for (i = 0; i < num_segs; i++) {
		for (j = 0; j < tr_vec_buff[i].num_tr; j++) {
			tr_vec_buff[i].tr[j].sim /= tr_vec_buff[i].tr[0].sim;  // normalize in [0,1]
			tr_vec_buff[i].tr[j].trtup_unused = 0;
			node = (TR_TUP**) tsearch(&tr_vec_buff[i].tr[j], &scoreTree, comp_did);
			if (node) {
				(*node)->sim += tr_vec_buff[i].tr[j].sim;
				(*node)->trtup_unused++;
			}
		}
	}
	numFuseRes = 0;
	twalk(scoreTree, storeSim);
	fused_tr_vec->num_tr = MIN(top, numFuseRes);
	qsort(fused_tr_tup_buff, fused_tr_vec->num_tr, sizeof(TR_TUP), comp_combmnz);

	for (i = 0; i < fused_tr_vec->num_tr; i++)
		fused_tr_tup_buff[i].rank = i+1;

	fused_tr_vec->tr = fused_tr_tup_buff;

	// Free the tr_tups of tr_vec_buffs
	for (i = 0; i < num_segs; i++) {
		Free(tr_vec_buff[i].tr);
	}
	return 1;
}

int fuseResultsByNSimMerging(char* thisfile, int num_segs, TR_VEC* fused_tr_vec)
{
	int i = 0, j = 0, k = 0, maxsimIndex;
	static char key[32];
	float  maxSim;
	ENTRY e, *ep;
	TR_TUP** buffp;

	// Hash table for detecting duplicate documents
	hcreate(top);

	fused_tr_vec->qid = tr_vec_buff[0].qid;

	// Normaliaze the similarity scores in [0,1] by dividing each score by the max in the list
	// No need to resort since these lists are already sorted
	for (i = 0; i < num_segs; i++) {
		maxSim = tr_vec_buff[i].tr[0].sim;
		for (j = 0; j < tr_vec_buff[i].num_tr; j++) {
			tr_vec_buff[i].tr[j].sim /= maxSim;
		}
	}

	// Now merge these lists. Keep num_segs pointers into the buffers
	buffp = (TR_TUP**) malloc (num_segs * sizeof(TR_TUP*));
	for (i = 0; i < num_segs; i++)
		buffp[i] = tr_vec_buff[i].tr;

	while (k < top) {
		// Find the minimum sim of the buffp pointee values
		maxsimIndex = -1;
		maxSim = 0;
		for (i = 0; i < num_segs; i++) {
			if (buffp[i] - tr_vec_buff[i].tr >= tr_vec_buff[i].num_tr)
				continue;
			if (buffp[i]->sim > maxSim) {
				maxsimIndex = i;
				maxSim = buffp[i]->sim;
			}	
		}
		if (maxsimIndex == -1)
			break;

		snprintf(key, sizeof(key), "%ld", buffp[maxsimIndex]->did);
		e.key = key;
		if (NULL == (ep = hsearch(e, FIND))) {
			if (NULL == (ep = hsearch(e, ENTER))) return UNDEF;
			// Put the maxsim value into the merged output
			fused_tr_tup_buff[k] = *(buffp[maxsimIndex]);
			fused_tr_tup_buff[k].rank = k+1;
		   	k++;	
		}
		buffp[maxsimIndex]++;
	}

	hdestroy();

	fused_tr_vec->tr = fused_tr_tup_buff;
	fused_tr_vec->num_tr = k;

	// Free the tr_tups of tr_vec_buffs
	for (i = 0; i < num_segs; i++) {
		Free(tr_vec_buff[i].tr);
	}
	return 1;
}

int
close_tr_tr_interleave (inst)
int inst;
{
	PRINT_TRACE (2, print_string, "Trace: entering close_tr_tr_interleave");

    if (UNDEF == close_textloc (textloc_index))
        return (UNDEF);

	Free(tr_vec_buff);
	Free(fused_tr_tup_buff);

	PRINT_TRACE (2, print_string, "Trace: leaving close_tr_tr_interleave");
	return (0);
}


int get_rank(const void *x) {
	TR_TUP* trtup = (TR_TUP*)x;
	return trtup->rank;
}

static int comp_did (tr1, tr2)
TR_TUP *tr1;
TR_TUP *tr2;
{
    if (tr1->did > tr2->did) return 1;
    if (tr1->did < tr2->did) return -1;

    return 0;
}


int 
comp_sim (tr1, tr2)
TR_TUP *tr1;
TR_TUP *tr2;
{
    if (tr1->sim > tr2->sim) return -1;
    if (tr1->sim < tr2->sim) return 1;

    return 0;
}

int 
comp_combanz (tr1, tr2)
TR_TUP *tr1;
TR_TUP *tr2;
{
    if (tr1->sim/(float)tr1->trtup_unused > tr2->sim/(float)tr2->trtup_unused) return -1;
    if (tr1->sim/(float)tr1->trtup_unused < tr2->sim/(float)tr2->trtup_unused) return 1;

    return 0;
}

int 
comp_combmnz (tr1, tr2)
TR_TUP *tr1;
TR_TUP *tr2;
{
    if (tr1->sim*tr1->trtup_unused > tr2->sim*tr2->trtup_unused) return -1;
    if (tr1->sim*tr1->trtup_unused < tr2->sim*tr2->trtup_unused) return 1;

    return 0;
}

