#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/text_tr_o.c,v 11.2 1993/02/03 16:49:26 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *  Take a text file in TREC++ format(focussed INEX submission), and
 *  convert it to a RIC run.
 *  Warning: Doesn't detect overlaps. So, the input shouldn't comprise
 *  of overlapping passages.
***********************************************************************/

#include <ctype.h>
#include <search.h>
#include "common.h"
#include "param.h"
#include "io.h"
#include "functions.h"
#include "spec.h"
#include "trace.h"
#include "smart_error.h"
#include "textline.h"


static char *default_sub_file;
static char* runid;
static char* grpby_method;
static char* mode;
static long buffSize = 2048;
static long num_wanted;
static int  restricted;
static int  num_chars_wanted;	// Number of chars wanted per article

static SPEC_PARAM spec_args[] = {
    {"text_inex_fcs_to_ric.out_file", getspec_dbfile, (char *) &default_sub_file},
    {"text_inex_fcs_to_ric.runid",    getspec_string, (char *) &runid},
    {"text_inex_fcs_to_ric.mode",    getspec_string, (char *) &mode},
    {"text_inex_fcs_to_ric.grpbymethod",    getspec_string, (char *) &grpby_method},
    {"text_inex_fcs_to_ric.restriction",    getspec_bool, (char *) &restricted},
    {"text_inex_fcs_to_ric.num_chars_wanted",    getspec_long, (char *) &num_chars_wanted},
    {"text_inex_fcs_to_ric.num_wanted",    getspec_long, (char *) &num_wanted},
    TRACE_PARAM ("text_inex_fcs_to_ric.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static void groupByReadingOrder();	// eliminate overlaps for a single query
static void groupByAggregateSim();
static void writeRecords(FILE* );
static int  comp_grp_sim();
static int  numRcds = 0;

typedef struct {
	long  qid;
	long  did;
	int   rank;
	float sim;
	int   start;
	int   end;
	char  deleted;
	float grpSim;
} InexSubmissionRcd;

static InexSubmissionRcd* inexSubmissionRds;

int
init_text_inex_fcs_to_ric (spec, unused)
SPEC *spec;
char *unused;
{
    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_text_inex_fcs_to_ric");

	inexSubmissionRds = (InexSubmissionRcd*) malloc (buffSize * sizeof(InexSubmissionRcd));

    PRINT_TRACE (2, print_string, "Trace: leaving init_text_inex_fcs_to_ric");

    return (0);
}

int
text_inex_fcs_to_ric (text_file, out_file, inst)
char *text_file;
char *out_file;
int inst;
{
    FILE *in_fd, *out_fd;
    char line_buf[PATH_LEN];
    SM_TEXTLINE textline;
    char *token_array[7];
	int  start;

	InexSubmissionRcd* rcd;
    long qid, prevqid = -1;

    PRINT_TRACE (2, print_string, "Trace: entering text_inex_fcs_to_ric");

    if (VALID_FILE (text_file)) {
        if (NULL == (in_fd = fopen (text_file, "r")))
            return (UNDEF);
    }
    else
        in_fd = stdin;

    if (out_file == (char *) NULL)
        out_file = default_sub_file;

    if (NULL == (out_fd = fopen(out_file, "w"))) {
		return UNDEF;
    }

    textline.max_num_tokens = 8;
    textline.tokens = token_array;
    
    while (NULL != fgets (line_buf, PATH_LEN, in_fd)) {
        /* Break line_buf up into tokens */
        (void) text_textline (line_buf, &textline);
        if (textline.num_tokens < 8) {
            set_error (SM_ILLPA_ERR, "Illegal input line", "text_inex_fcs_to_ric");
            return (UNDEF);
        }
		start = atoi(textline.tokens[6]);
		if (start < 0)
			continue;

        qid = atol (textline.tokens[0]);
        if (qid != prevqid && prevqid > 0) {
			/* Start of a new query */
			prevqid = qid;
			writeRecords(out_fd);
        }
		else if (prevqid < 0)
			prevqid = qid;

		rcd = inexSubmissionRds + numRcds;
		rcd->qid = qid;
		rcd->did = atol(textline.tokens[2]);
		rcd->rank = atoi(textline.tokens[3]);
		rcd->sim = atof(textline.tokens[4]);
		rcd->start = start; 
		rcd->end = rcd->start + atoi(textline.tokens[7]) - 1;
		rcd->deleted = 0;

		numRcds++;

        if (numRcds >= buffSize) {
			buffSize = buffSize<<1;
            if (NULL == (inexSubmissionRds = (InexSubmissionRcd *)
                    realloc (inexSubmissionRds, buffSize * sizeof (InexSubmissionRcd))))
                return (UNDEF);
        }
    }

	writeRecords(out_fd);

    if (VALID_FILE (text_file))
        (void) fclose (in_fd);
    if (VALID_FILE (out_file))
        (void) fclose (out_fd);

    PRINT_TRACE (2, print_string, "Trace: leaving text_inex_fcs_to_ric");
    return (1);
}

int
close_text_inex_fcs_to_ric (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_text_inex_fcs_to_ric");

	free(inexSubmissionRds);

    PRINT_TRACE (2, print_string, "Trace: leaving close_text_inex_fcs_to_ric");
    
    return (0);
}

/* Write the records (restricted to num_chars_wanted per article */
static void writeRecords(FILE* out_fd)
{
	int i, n;
	int num_chars_written = 0, length;
	int this_did = -1;

	InexSubmissionRcd* rcd;

	if (*mode == 'r') {
		if (!strcmp(grpby_method, "reading_order"))
			groupByReadingOrder();
		else
			groupByAggregateSim();
	}

	n = MIN(numRcds, num_wanted);
	// Write out the overlap eliminated records in the output file
	for (i = 0; i < n; i++) {
		rcd = &inexSubmissionRds[i];
		if (*mode == 'r') {
			if (this_did < 0)
				this_did = rcd->did;
			else if (rcd->did != this_did) {
				// We have got a new article starting from here. Reset num_chars_written
				num_chars_written = 0;
				this_did = rcd->did;
			}
		}

		if (rcd->start == -1)
			continue;
		if (!rcd->deleted) {
			length = rcd->end - rcd->start + 1;
			num_chars_written += length;
			if (restricted && num_chars_written > num_chars_wanted) {
				// Undo adding this article
				num_chars_written -= length;
				continue;
			}
			fprintf(out_fd, "%d Q0 %d %d %f %s %d %d\n", rcd->qid, rcd->did, rcd->rank, rcd->sim, runid,
						rcd->start, length);
		}
	}
	numRcds = 0;
}


static void groupByReadingOrder()
{
	int i, j, k, l;
	InexSubmissionRcd tmp;

	for (i = 0; i < numRcds; i++) {
		k = i+1;
		while (inexSubmissionRds[k++].did == inexSubmissionRds[i].did);
		k--;
		for (j = k+1; j < numRcds; j++) {
			if (inexSubmissionRds[i].did != inexSubmissionRds[j].did) continue;	// If records from different articles don't bother
			// Insert rcd[j] in the array and shift all the elements down. This is required
			// to maintain the sorted property of the array.
			l = j;
			tmp = inexSubmissionRds[j];
			while (l > k) {
				inexSubmissionRds[l] = inexSubmissionRds[l-1];
				l--;
			}
			inexSubmissionRds[k++] = tmp;
		}
	}
}

static void groupByAggregateSim()
{
	int i;
	InexSubmissionRcd *thisElt, *foundEltSim;
	ENTRY        elt, *foundElt;
	char key[16];

	hcreate(numRcds);

	// Insert the keys 
	for (i = 0; i < numRcds; i++) {
		thisElt = &inexSubmissionRds[i];
		thisElt->grpSim = 0;
		snprintf(key, sizeof(key), "%d", thisElt->did); 
		elt.key = key;
		elt.data = (void*)thisElt;
		assert(hsearch(elt, ENTER));
	}

	// Accumulate group score for elements belonging to the same article
	for (i = 0; i < numRcds; i++) {
		thisElt = &inexSubmissionRds[i];
		snprintf(key, sizeof(key), "%d", thisElt->did); 
		elt.key = key;
		elt.data = (void*)thisElt;
		foundElt = hsearch(elt, FIND);
		if (foundElt) {
			foundEltSim = (InexSubmissionRcd*)foundElt->data;
			foundEltSim->grpSim += ((numRcds - i) / (float)numRcds) * thisElt->sim;	// add up the similarities
		}
	}
	
	// One more pass to write the accumulated grp sim value to all the member elements	
	for (i = 0; i < numRcds; i++) {
		thisElt = &inexSubmissionRds[i];
		snprintf(key, sizeof(key), "%d", thisElt->did); 
		elt.key = key;
		elt.data = (void*)thisElt;
		foundElt = hsearch(elt, FIND);
		if (foundElt) {
			foundEltSim = (InexSubmissionRcd*)foundElt->data;
			if (foundEltSim->grpSim > 0)
				thisElt->grpSim = foundEltSim->grpSim;
		}
	}

	// Sort the objects by the grp similarity
    qsort(inexSubmissionRds, numRcds, sizeof(InexSubmissionRcd), comp_grp_sim);
	hdestroy();
}

static int 
comp_grp_sim (e1, e2)
const void *e1;
const void *e2;
{
	long el1_did, el2_did;
    InexSubmissionRcd *es1 = (InexSubmissionRcd *) e1;
    InexSubmissionRcd *es2 = (InexSubmissionRcd *) e2;

    if (es1->grpSim > es2->grpSim) return -1;
    if (es1->grpSim < es2->grpSim) return 1;

	el1_did = es1->did;
	el2_did = es2->did;

	if (el1_did < el2_did) return -1;
	if (el1_did > el2_did) return 1;
	
	return -1;
}
