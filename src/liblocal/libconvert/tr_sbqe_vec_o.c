#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/tr_sbqe_o.c,v 11.0 1992/07/21 18:20:09 chrisb Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.

   Implement the Sentence Based Query Expansion (SBQE).
   Input:  init retr. tr_file, query vector
   Output: a vector file representing the expanded query
   Outline: Read the top pseudo-relevant documents from the tr file, open each document text,
            get the sentences or pseudo-sentences (if fixed length word window mode is enabled),
			compute the similarities of each (pseudo)sentence with the query vector with the
			desired similarity and add the sentence to the query.

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

static char *query_file;
static long query_file_mode;
static long tr_mode;
static long expanded_query_file_mode;
static long maxSentences;
static int incremental;
static int vns;

static SPEC_PARAM spec_args[] = {
    {"convert.tr_sbqe.query_file", getspec_dbfile, (char *) &query_file},
    {"convert.tr_sbqe.tr_file.rmode", getspec_filemode, (char *) &tr_mode},
    {"convert.tr_sbqe.query_file.rmode", getspec_filemode, (char *) &query_file_mode},
    {"convert.tr_sbqe.expanded_query_file.rwcmode", getspec_filemode, (char *) &expanded_query_file_mode},
    {"convert.tr_sbqe.maxsentences",getspec_long, (char *) &maxSentences},
    {"convert.tr_sbqe.incremental",getspec_bool, (char *) &incremental},
    {"convert.tr_sbqe.vns",getspec_bool, (char *) &vns},
    TRACE_PARAM ("convert.tr_sbqe.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

	int qvec_fd; 
    int tr_sbqe_inst;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int init_tr_sbqe(), tr_sbqe(), close_tr_sbqe();

int
init_tr_sbqe_obj (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_tr_sbqe_obj");
    
    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];
    if (!VALID_FILE (query_file)) {
		return UNDEF;
	}

	// Open the output expanded query vector for writing out the expanded query
    if (UNDEF == (ip->qvec_fd = open_vector (query_file, query_file_mode)))
        return (UNDEF);
    if (UNDEF == (ip->tr_sbqe_inst = init_tr_sbqe(spec, "doc.")))
        return UNDEF;

    ip->valid_info = 1;
             
    PRINT_TRACE (2, print_string, "Trace: leaving init_tr_sbqe_obj");
    
    return (new_inst);
}


int
tr_sbqe_obj (tr_file, outfile, inst)
char *tr_file;
char *outfile;
int inst;
{
    STATIC_INFO *ip;
    VEC expqvec, qvec, prev_expqvec, newexpqvec;
    long *dids;
    long i, num_dids = 0;
	TR_VEC tr_vec;
	int eqvec_fd;
	int tr_fd;
	EID eid;
	int m;
	static char msg[64];

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "tr_sbqe_obj");
        return (UNDEF);
    }
    ip  = &info[inst];

	PRINT_TRACE (2, print_string, "Trace: entering tr_sbqe_obj");

    if (UNDEF == (tr_fd = open_tr_vec (tr_file, tr_mode)))
		return UNDEF;
    if (UNDEF == (eqvec_fd = open_vector (outfile, expanded_query_file_mode)))
        return (UNDEF);

	/* Iterate over all tr_vec records and read the current query */
    while (1 == read_tr_vec (tr_fd, &tr_vec)) {
		// Read the query vector
		qvec.id_num.id = tr_vec.qid;
	    if (UNDEF == seek_vector(ip->qvec_fd, &qvec) ||
		    UNDEF == read_vector(ip->qvec_fd, &qvec))
			return(UNDEF);

		copy_vec(&prev_expqvec, &qvec);
	    for (i = 0; i < tr_vec.num_tr; i++) {
	    	eid.id = tr_vec.tr[i].did;

			m = !vns? maxSentences : maxSentences + ((1 - maxSentences)/(double)(tr_vec.num_tr - 1)) * (tr_vec.tr[i].rank-1);
			snprintf(msg, sizeof(msg), "Selecting %d sentences from %dth document (id= %d).", m, tr_vec.tr[i].rank, eid.id);
    		PRINT_TRACE (4, print_string, msg);

			if (incremental) {
				if (UNDEF == tr_sbqe(eid, &prev_expqvec, &expqvec, m, ip->tr_sbqe_inst))
					return (UNDEF);
			}
			else {
				if (UNDEF == tr_sbqe(eid, &qvec, &expqvec, m, ip->tr_sbqe_inst))
					return (UNDEF);
			}

			if (incremental) {
				free_vec(&prev_expqvec);
				copy_vec(&prev_expqvec, &expqvec);
			}
			else {
				// Merge the new expanded query vector with the query vector
				// obtained at the previous step.
				merge_vec(&prev_expqvec, &expqvec, &newexpqvec);	// newexpqvec <- prev_expqvec + expqvec
				free_vec(&prev_expqvec); 	
				copy_vec(&prev_expqvec, &newexpqvec);
				free_vec(&newexpqvec); 	
			}
			free_vec(&expqvec);
		}
		if (UNDEF == seek_vector (eqvec_fd, &prev_expqvec) ||
			UNDEF == write_vector (eqvec_fd, &prev_expqvec))
			return (UNDEF);
		free_vec(&prev_expqvec);
	}

    if (UNDEF == close_vector (eqvec_fd))
        return (UNDEF);
    if (UNDEF == close_tr_vec (tr_fd))
		return UNDEF;

    PRINT_TRACE (4, print_vector, qvec);
    PRINT_TRACE (2, print_string, "Trace: leaving tr_sbqe_obj");
    return (1);
}


int
close_tr_sbqe_obj(inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_tr_sbqe_obj");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_tr_sbqe_obj");
        return (UNDEF);
    }

    ip  = &info[inst];
    ip->valid_info = 0;

    if (UNDEF == close_vector (ip->qvec_fd))
        return (UNDEF);
    if (UNDEF == close_tr_sbqe(ip->tr_sbqe_inst))
        return UNDEF;

    PRINT_TRACE (2, print_string, "Trace: leaving close_tr_sbqe_obj");
    return (0);
}
