#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/text_tr_o.c,v 11.2 1993/02/03 16:49:26 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *  Take a text file in TREC++ format, read it in memory for each query
 *  and remove the overlapping passages. After removing overlapping
 *  passages, keep the top num_wanted ones.
 *  Input must be sorted by qid.
***********************************************************************/

#include <ctype.h>
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
static long buffSize = 2048;
static long num_wanted;

static SPEC_PARAM spec_args[] = {
    {"text_inexfolsub.sub_file", getspec_dbfile, (char *) &default_sub_file},
    {"text_inexfolsub.runid",    getspec_string, (char *) &runid},
    {"text_inexfolsub.num_wanted",    getspec_long, (char *) &num_wanted},
    TRACE_PARAM ("text_inexfolsub.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static void eliminateOverlaps();	// eliminate overlaps for a single query
static int numRcds = 0;

typedef struct {
	long  qid;
	long  did;
	int   rank;
	float sim;
	int   start;
	int   end;
	char  deleted;
} InexSubmissionRcd;

static InexSubmissionRcd* inexSubmissionRds;

int
init_text_inexfolsub (spec, unused)
SPEC *spec;
char *unused;
{
    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_text_inexfolsub");

	inexSubmissionRds = (InexSubmissionRcd*) malloc (buffSize * sizeof(InexSubmissionRcd));

    PRINT_TRACE (2, print_string, "Trace: leaving init_text_inexfolsub");

    return (0);
}

int
text_inexfolsub (text_file, out_file, inst)
char *text_file;
char *out_file;
int inst;
{
    FILE *in_fd, *out_fd;
    char line_buf[PATH_LEN];
    SM_TEXTLINE textline;
    char *token_array[7];

	InexSubmissionRcd* rcd;
    long qid, prevqid = -1;
	int  i, n;

    PRINT_TRACE (2, print_string, "Trace: entering text_inexfolsub");

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
            set_error (SM_ILLPA_ERR, "Illegal input line", "text_inexfolsub");
            return (UNDEF);
        }
        qid = atol (textline.tokens[0]);
        if (qid != prevqid && prevqid > 0) {
			/* Start of a new query */
			eliminateOverlaps();
			prevqid = qid;

			// Write out the overlap eliminated records in the output file
			n = MIN(num_wanted, numRcds); 
			for (i = 0; i < n; i++) {
				rcd = &inexSubmissionRds[i];
				if (!rcd->deleted)
					fprintf(out_fd, "%d Q0 %d %d %f %s %d %d\n", rcd->qid, rcd->did, rcd->rank, rcd->sim, runid,
								rcd->start, rcd->end - rcd->start + 1);
			}
			numRcds = 0;
        }
		else if (prevqid < 0)
			prevqid = qid;

		rcd = inexSubmissionRds + numRcds;
		rcd->qid = qid;
		rcd->did = atol(textline.tokens[2]);
		rcd->rank = atoi(textline.tokens[3]);
		rcd->sim = atof(textline.tokens[4]);
		rcd->start = atoi(textline.tokens[6]);
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
	// Write the last set
	eliminateOverlaps();

	// Write out the overlap eliminated records in the output file
	n = MIN(num_wanted, numRcds); 
	for (i = 0; i < n; i++) {
		rcd = &inexSubmissionRds[i];
		if (!rcd->deleted)
			fprintf(out_fd, "%d Q0 %d %d %f %s %d %d\n", rcd->qid, rcd->did, rcd->rank, rcd->sim, runid,
						rcd->start, rcd->end - rcd->start + 1);
	}

    if (VALID_FILE (text_file))
        (void) fclose (in_fd);
    if (VALID_FILE (out_file))
        (void) fclose (out_fd);

    PRINT_TRACE (2, print_string, "Trace: leaving text_inexfolsub");
    return (1);
}

int
close_text_inexfolsub (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_text_inexfolsub");

	free(inexSubmissionRds);

    PRINT_TRACE (2, print_string, "Trace: leaving close_text_inexfolsub");
    
    return (0);
}

int isOverlap(InexSubmissionRcd* this, InexSubmissionRcd* that)
{
	int thisContainedInThat = this->start >= that->start && this->end <= that->end;
	int thatContainedInThis = this->start <= that->start && this->end >= that->end;
	int thisIntersectsThat = ISBETWEEN(that->start, this->start, this->end) || ISBETWEEN(that->end, this->start, this->end);

	return thisContainedInThat || thatContainedInThis || thisIntersectsThat;
}

static void eliminateOverlaps()
{
	int i, j;
	InexSubmissionRcd *this, *that, *rcdToDelete;

	for (i = 0; i < numRcds; i++) {
		this = inexSubmissionRds + i;
		if (this->start < 0) continue;
		if (this->deleted) continue;

		for (j = 0; j < numRcds; j++) {

			if (i == j) continue;
			that = inexSubmissionRds + j;
			if (that->start < 0) continue;
			if (that->deleted) continue;
			if (this->did != that->did) continue;	// Don't detect overlap across docs

			if (isOverlap(this, that)) {
				// Remove 'that'. This is done by replacing 'that' with the last object
				// and decrementing numRcds by 1. Note this doesn't violate the sorted order.
				rcdToDelete = this->sim < that->sim ? this : that; 
				rcdToDelete->deleted = 1;
			}
		}
	}
}
