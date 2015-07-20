#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/tr_inexqa_o.c,v 11.0 1992/07/21 18:20:09 chrisb Exp $";
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
#include "local_eid.h"
#include "textloc.h"

static char *query_file;
static long query_file_mode;
static long tr_mode;
static PROC_TAB *sent_sel;
static int num_wanted;
static int num_conwt_min_threshold = 5;
static int num_conwt_max_threshold = 50;
static PROC_TAB *index_pp;
static char* textloc_file;
static int textloc_mode;
static char* runName;
static int textloc_index;

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int 	 valid_info;
	int      index_pp_inst;
	Sentence* sent_vecs;  
	int      sent_vecs_buff_size;
	int      textdoc_inst;
	int      print_vec_inst;
	int 	 qvec_fd; 
} STATIC_INFO;

static SPEC_PARAM spec_args[] = {
    PARAM_FUNC("convert.tr_inexqa.index_pp", &index_pp),
    {"convert.tr_inexqa.query_file", getspec_dbfile, (char *) &query_file},
    {"convert.tr_inexqa.tr_file.rmode", getspec_filemode, (char *) &tr_mode},
    {"convert.tr_inexqa.query_file.rmode", getspec_filemode, (char *) &query_file_mode},
    {"convert.tr_inexqa.sent_sel",getspec_func, (char *) &sent_sel},	/* how the sentences are to be selected - lm, rlm etc. */
    {"convert.tr_inexqa.num_sentences_wanted",getspec_int, (char *) &num_wanted},
    {"convert.tr_inexqa.textloc_file",     getspec_dbfile, (char *) &textloc_file},
    {"convert.tr_inexqa.textloc_file.rmode",getspec_filemode, (char *) &textloc_mode},
    {"convert.tr_inexqa.run_name",getspec_string, (char *) &runName},
    TRACE_PARAM ("convert.tr_inexqa.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static STATIC_INFO *info;
static int max_inst = 0;
static int compare_sentence(Sentence* this, Sentence* that);

// Return the file name (without the .xml) from the full path name of a file
static char* getFileName(char* pathname) {
	static char buff[1024];
	int len;
	len = snprintf(buff, sizeof(buff), "%s", pathname);
	char* p = buff + len;
	while (p >= buff && *p != '/') {
		if (*p == '.') *p = 0;
		p--;
	}
	return p+1;
}

int init_tr_inexqa_obj (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;
    CONTEXT old_context;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_tr_inexqa_obj");
    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];
    if (!VALID_FILE (query_file)) {
		return UNDEF;
	}

    old_context = get_context();
    set_context (CTXT_DOC);

    if (UNDEF == (ip->textdoc_inst = init_get_textdoc(spec, NULL)))
		return UNDEF;

    add_context (CTXT_SENT);
    if (UNDEF == (ip->index_pp_inst = index_pp->init_proc(spec, "doc.sent.")))
		return UNDEF;

	ip->sent_vecs_buff_size = 65536;
	if (NULL == (ip->sent_vecs = Malloc(ip->sent_vecs_buff_size, Sentence)))
		return UNDEF;

    del_context (CTXT_SENT);
    set_context (old_context);

	// Open the output expanded query vector for writing out the expanded query
    if (UNDEF == (ip->qvec_fd = open_vector (query_file, query_file_mode)))
        return (UNDEF);
    if (UNDEF == sent_sel->init_proc(spec, "doc."))
        return UNDEF;

    if (UNDEF == (textloc_index = open_textloc (textloc_file, textloc_mode)))
        return (UNDEF);
    if (sm_trace >= 4 &&
        UNDEF == (ip->print_vec_inst = init_print_vec_dict(spec, NULL)))
        return UNDEF;

    ip->valid_info = 1;
    PRINT_TRACE (2, print_string, "Trace: leaving init_tr_inexqa_obj");
    return (new_inst);
}

int tr_inexqa_obj (char* tr_file, char* outfile, int inst)
{
    STATIC_INFO *ip;
    SM_INDEX_TEXTDOC pp_vec, pp_svec;
    VEC         qvec;
    long 		i, j, k = 0; 
	TR_VEC 		tr_vec;
	TR_TUP      *tr;
	int 		tr_fd, s, nbytes;
	EID 		eid;
	FILE*       outfd;
	static char msg[16192];
	TEXTLOC     textloc;
	char*       docname;

	if (outfile == NULL)
		return UNDEF;
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "tr_inexqa_obj");
        return (UNDEF);
    }
    ip  = &info[inst];

	PRINT_TRACE (2, print_string, "Trace: entering tr_inexqa_obj");

	outfd = fopen(outfile, "w");
	if (outfd == NULL)
		return UNDEF;

    if (UNDEF == (tr_fd = open_tr_vec (tr_file, tr_mode)))
		return UNDEF;

	/* Iterate over all tr_vec records and read the current query */
    while (1 == read_tr_vec (tr_fd, &tr_vec)) {
		// Read the query vector
		qvec.id_num.id = tr_vec.qid;
	    if (UNDEF == seek_vector(ip->qvec_fd, &qvec) ||
		    UNDEF == read_vector(ip->qvec_fd, &qvec))
			return(UNDEF);

		/* Collect the sentences of pseudo-relevant documents in a list of vectors
		 * to be used by the similarity calculator. */
	    for (tr = tr_vec.tr; tr < &tr_vec.tr[tr_vec.num_tr]; tr++) {
    		eid.id = tr->did;
			eid.ext.type = P_SENT;  // We intend to decompose the document into sentences

		    if (UNDEF == get_textdoc(eid.id, &pp_vec, ip->textdoc_inst))
				return(UNDEF);
		    pp_vec.mem_doc.id_num = eid;
			Bcopy(&pp_vec, &pp_svec, sizeof(SM_INDEX_TEXTDOC));

			for (i = 0; i < pp_vec.mem_doc.num_sections; i++) {
			    for (j = 0; j < pp_vec.mem_doc.sections[i].num_subsects; j++) {
	    		    pp_svec.mem_doc.sections = &(pp_vec.mem_doc.sections[i].subsect[j]);
					// The sentences are obtained under one more level. 
					if (pp_vec.mem_doc.sections[i].subsect[j].num_subsects <= 1)
						continue;
					for (s = 0; s < pp_vec.mem_doc.sections[i].subsect[j].num_subsects; s++) {
	    	    		pp_svec.mem_doc.sections = &(pp_vec.mem_doc.sections[i].subsect[j].subsect[s]);
						pp_svec.mem_doc.num_sections = 1;

						// check if we are exceeding the sent_vecs_buff_size
						if (k >= ip->sent_vecs_buff_size) {
							ip->sent_vecs_buff_size = twice(k);
							if (UNDEF == (ip->sent_vecs = Realloc(ip->sent_vecs, ip->sent_vecs_buff_size, Sentence)))
								return UNDEF;
						}
						ip->sent_vecs[k].did = tr->did;

		    			if (UNDEF == index_pp->proc(&pp_svec, &(ip->sent_vecs[k].svec), ip->index_pp_inst))
			    			return UNDEF;
						// Efficient relocation of this vector after sorting by the similarity values
						if (UNDEF == save_vec(&(ip->sent_vecs[k].svec)))
							return(UNDEF);
						// Skip very long sentences....
						if (ip->sent_vecs[k].svec.num_conwt < num_conwt_min_threshold ||
							ip->sent_vecs[k].svec.num_conwt > num_conwt_max_threshold) {
							free_vec(&(ip->sent_vecs[k].svec));
							continue;
						}
						// save the text of this sentence in this sentence object...
						nbytes = pp_svec.mem_doc.sections[0].end_section - pp_svec.mem_doc.sections[0].begin_section;
						if (nbytes >= sizeof(ip->sent_vecs[k].text) - 1) {
							free_vec(&(ip->sent_vecs[k].svec));
							continue;
						}
						strncpy(ip->sent_vecs[k].text, &pp_svec.doc_text[pp_svec.mem_doc.sections[0].begin_section], nbytes);
						ip->sent_vecs[k].text[nbytes] = 0;

						snprintf(msg, sizeof(msg), "sentence[%d]:", k);
				    	PRINT_TRACE (4, print_string, msg);
						strncpy(msg, &pp_svec.doc_text[pp_svec.mem_doc.sections[0].begin_section],
								MIN(sizeof(msg)-1, nbytes));
			    		PRINT_TRACE (4, print_string, msg);

						snprintf(msg, sizeof(msg), "sentence vector[%d]:", k);
		    			PRINT_TRACE (4, print_string, msg);
        				if (sm_trace >= 4) {
	        				if (UNDEF == print_vec_dict(&(ip->sent_vecs[k].svec), NULL, ip->print_vec_inst))
	            				return UNDEF;
						}
						k++;
					}
				}
			}
		}

		/* Call the sentence selection procedure to report a list of top scoring sentences. */
		if (UNDEF == sent_sel->proc(ip->sent_vecs, k, &qvec, &tr_vec))
			return (UNDEF);
		qsort(ip->sent_vecs, k, sizeof(Sentence), compare_sentence);

		// Write out the answer text composed of sentences in TREC format
		for (i = 0; i < MIN(num_wanted, k); i++) {
			textloc.id_num = ip->sent_vecs[i].did;  
        	if (UNDEF == seek_textloc (textloc_index, &textloc) ||
    			UNDEF == read_textloc (textloc_index, &textloc))
				return UNDEF;
			docname = getFileName(textloc.file_name);
			nbytes = snprintf(msg, sizeof(msg), "%d\tQ0\t%s\t%d\t%f\t%s\t%s", qvec.id_num.id, docname,
							i+1, ip->sent_vecs[i].sim, runName, ip->sent_vecs[i].text);
			if (msg[nbytes-1] == '\n')
				msg[nbytes-1] = 0;
			fprintf(outfd, "%s\n", msg);
		}
	}

	for (i = 0; i < k; i++)
		free_vec(&(ip->sent_vecs[i].svec));

    if (UNDEF == close_tr_vec (tr_fd))
		return UNDEF;
	fclose(outfd);

    PRINT_TRACE (4, print_vector, qvec);
    PRINT_TRACE (2, print_string, "Trace: leaving tr_inexqa_obj");
    return (1);
}


int
close_tr_inexqa_obj(inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_tr_inexqa_obj");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_tr_inexqa_obj");
        return (UNDEF);
    }

    ip  = &info[inst];
    ip->valid_info = 0;

    if (UNDEF == index_pp->close_proc(ip->index_pp_inst))
   	    return UNDEF;			
	if (UNDEF == close_get_textdoc(ip->textdoc_inst))
   		return UNDEF;
    if (UNDEF == close_vector (ip->qvec_fd))
        return (UNDEF);
    if (UNDEF == sent_sel->close_proc())
        return UNDEF;
    if (UNDEF == close_textloc (textloc_index))
        return (UNDEF);

   	if (sm_trace >= 4 &&
       	UNDEF == close_print_vec_dict(ip->print_vec_inst))
        return UNDEF;

    PRINT_TRACE (2, print_string, "Trace: leaving close_tr_inexqa_obj");
    return (0);
}

// Descending order of sentence similarities
static compare_sentence(Sentence* this, Sentence* that) {
     return this->sim < that->sim ? 1 : this->sim == that->sim ? 0 : -1;
}

