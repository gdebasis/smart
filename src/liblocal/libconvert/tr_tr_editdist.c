#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/tr_sbqe_o.c,v 11.0 1992/07/21 18:20:09 chrisb Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.

   Work submitted to CIKM 2012.
   Get the document text and query text. Compute the edit distance between two
   and generate a reranked tr file based on the distances computed.
*/

#include <fcntl.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "context.h"
#include "retrieve.h"
#include "vector.h"
#include "docindex.h"
#include "inst.h"
#include "local_eid.h"

static char *query_file;
static long query_file_mode;
static long tr_mode;

static SPEC_PARAM spec_args[] = {
    {"convert.tr_tr_editdist.query_file", getspec_dbfile, (char *) &query_file},
    {"convert.tr_tr_editdist.tr_file.rmode", getspec_filemode, (char *) &tr_mode},
    {"convert.tr_tr_editdist.query_file.rmode", getspec_filemode, (char *) &query_file_mode},
    TRACE_PARAM ("convert.tr_tr_editdist.trace")
};
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;
	int qvec_fd; 
	int textdoc_inst;
	int textqry_inst;
}
STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

char* showText(SM_INDEX_TEXTDOC* pp_vec, int context);

int init_tr_tr_editdist (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_tr_tr_editdist");
    
    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];
    if (!VALID_FILE (query_file)) {
		return UNDEF;
	}

	// Open query vec file and tr file...
    if (UNDEF == (ip->qvec_fd = open_vector (query_file, query_file_mode)))
        return (UNDEF);

	// Init procs for opening the text of doc and qry...
    if (UNDEF == (ip->textdoc_inst = init_get_textdoc(spec, NULL)))
		return UNDEF;
    if (UNDEF == (ip->textqry_inst = init_get_textqry(spec, NULL)))
		return UNDEF;

    ip->valid_info = 1;
             
    PRINT_TRACE (2, print_string, "Trace: leaving init_tr_tr_editdist");
    return (new_inst);
}

int
tr_tr_editdist (tr_file, outfile, inst)
char *tr_file;
char *outfile;
int inst;
{
    STATIC_INFO *ip;
	SM_INDEX_TEXTDOC pp_vec, pp_qvec;
    VEC qvec;
	TR_VEC tr_vec;
	int tr_fd, i, j;
	EID eid;

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "tr_tr_editdist");
        return (UNDEF);
    }
    ip  = &info[inst];

	PRINT_TRACE (2, print_string, "Trace: entering tr_tr_editdist");

    if (UNDEF == (tr_fd = open_tr_vec (tr_file, tr_mode)))
		return UNDEF;

	eid.ext.type = ENTIRE_DOC;
	/* Iterate over all tr_vec records and read the current query */
    while (1 == read_tr_vec (tr_fd, &tr_vec)) {
		// Read the query vector
		qvec.id_num.id = tr_vec.qid;
	    if (UNDEF == seek_vector(ip->qvec_fd, &qvec) ||
		    UNDEF == read_vector(ip->qvec_fd, &qvec))
			return(UNDEF);
  		if (UNDEF == get_textqry(qvec.id_num.id, &pp_qvec, ip->textqry_inst))
			return(UNDEF);

		printf("Query: %s\n", showText(&pp_qvec, CTXT_QUERY));
	    for (i = 0; i < tr_vec.num_tr; i++) {
	  	 	eid.id = tr_vec.tr[i].did;
    		if (UNDEF == get_textdoc(eid.id, &pp_vec, ip->textdoc_inst))
				return(UNDEF);
			printf("%s\n", showText(&pp_vec, CTXT_DOC));
		}
	}

    if (UNDEF == close_tr_vec (tr_fd))
		return UNDEF;

    PRINT_TRACE (4, print_vector, qvec);
    PRINT_TRACE (2, print_string, "Trace: leaving tr_tr_editdist");
    return (1);
}

char* showText(SM_INDEX_TEXTDOC* pp, int context) {
	static char buff[8192];
	int j = pp->mem_doc.num_sections - 1, num_chars;
	num_chars = pp->mem_doc.sections[j].end_section - pp->mem_doc.sections[j].begin_section;
	num_chars = context == CTXT_DOC? num_chars-15 : num_chars; // Get rid of </TEXT></DOC> for a doc text
	if (num_chars < sizeof(buff)) {
		memcpy(buff, pp->doc_text + pp->mem_doc.sections[j].begin_section, num_chars);
		buff[num_chars] = 0;
	}
	return buff;
}


int
close_tr_tr_editdist(inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_tr_tr_editdist");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_tr_tr_editdist");
        return (UNDEF);
    }

    ip  = &info[inst];
    ip->valid_info = 0;

    if (UNDEF == close_vector (ip->qvec_fd))
        return (UNDEF);
	if (UNDEF == close_get_textdoc(ip->textdoc_inst))
		return UNDEF;
	if (UNDEF == close_get_textqry(ip->textqry_inst))
 		return UNDEF;

    PRINT_TRACE (2, print_string, "Trace: leaving close_tr_tr_editdist");
    return (0);
}
