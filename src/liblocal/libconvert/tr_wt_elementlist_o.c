#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libconvert/tr_tr_xml.c,v 11.2 1993/02/03 16:51:43 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Take a tr file and return a ranked list of XML elements (instead of docs)
 *1 local.convert.obj.tr_elts

 *9 See pp_xml.c:
 *9 Dirty hack: for each section (SM_DISP_SEC), subsection pointer (subsect) 
 *9   made to point to the XML tree node for the section. 

***********************************************************************/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "dict.h"
#include "docindex.h"
#include "functions.h"
#include "io.h"
#include "proc.h"
#include "smart_error.h"
#include "spec.h"
#include "tr_vec.h"
#include "trace.h"
#include "vector.h"
#include "context.h"
#include <search.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#define VERBOSE 0

#define HEADER_STRING \
		"<inex-submission participant-id=\"19\" run-id=\"%s\" task=\"Focused\" query=\"automatic\" result-type=\"element\">\n" \
"<topic-fields title=\"yes\" mmtitle=\"no\" castitle=\"no\" description=\"yes\" narrative=\"no\" />\n" \
"<description>Content-only approach using straightforward term-weighted vectors and inner-product similarity with query expansion.\n</description>\n" \
"<collections>\n  <collection>wikipedia</collection>\n</collections>\n"

typedef struct {
#if (VERBOSE > 0)
    long did, rank;
#endif
    char filename[PATH_LEN/4];
    unsigned char *path;
    long section_num;
    float sim;
	long  num_chars;	// size of text
	long  start_offset; // offset in the text from where the section starts
	long  grpSim;		// to be used for accumulating the group level similarities either by summing or by max.
} ELEMENT_SIM;

static char* run_id;
static char *default_tr_file;
static long num_wanted;
static long tr_mode;
static PROC_TAB *pp,               /* Preparse procedures */
    *index_pp;                     /* Indexing procedure */
static int qid_vec_inst, pp_inst, index_pp_inst, vv_inst, print_vec_inst, elt_wt_proc_inst;
static ELEMENT_SIM *element_sims;
static int max_elements, num_elements;
static char *textloc_file;
static long textloc_mode;
static long num_topdocs;		// Compute element level similarity only for these many docs
static long format;
static PROC_TAB *elt_weight_vec;	// To convert the elements into a desired weighting scheme
									// after the element vector is returned by index_pp

// For reading in the document vector used for retrieval
static char *vec_file;
static long vec_file_mode;
static int  vec_fd;

static float  lambda, mu;
static int    reweight_on_wholedoc;
static char*  mode;   // (t <=> thorough, f <=> focussed, r <=> relevant-in-context)
static long   docPriorSgn;

static SPEC_PARAM spec_args[] = {
    {"tr_elementlist.doc_file",        getspec_dbfile, (char *) &vec_file},
    {"tr_elementlist.doc_file.rmode",  getspec_filemode, (char *) &vec_file_mode},
    {"tr_elementlist.run_id",       getspec_string,   (char *) &run_id},
    {"tr_elementlist.in_tr_file", getspec_dbfile, (char *) &default_tr_file},
    {"tr_elementlist.in_tr_file.rmode", getspec_filemode, (char *) &tr_mode},
    {"tr_elementlist.num_wanted", getspec_long, (char *) &num_wanted},

    {"tr_elementlist.named_preparse", getspec_func, (char *) &pp},
    {"tr_elementlist.index_pp", getspec_func, (char *) &index_pp},

    {"tr_elementlist.textloc_file", getspec_dbfile, (char *) &textloc_file},
    {"tr_elementlist.textloc_file.rmode", getspec_filemode, (char *) &textloc_mode},
    {"tr_elementlist.num_topdocs", getspec_long, (char *) &num_topdocs},
    {"tr_elementlist.submission_format", getspec_long, (char *) &format},
    {"tr_elementlist.weight",    getspec_func,      (char *) &elt_weight_vec},
    {"tr_elementlist.lm.lambda",    getspec_float,      (char *) &lambda},
    {"tr_elementlist.lm.mu",    getspec_float,      (char *) &mu},
    {"tr_elementlist.lm.reweight_on_wholedoc",    getspec_bool,      (char *) &reweight_on_wholedoc},
    {"tr_elementlist.lm.docprior",    getspec_long,      (char *) &docPriorSgn},
    {"tr_elementlist.mode",    getspec_string,      (char *) &mode},
    TRACE_PARAM ("tr_elementlist.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


extern int init_sim_vec_vec(), sim_vec_vec(), close_sim_vec_vec();

static int eliminate_overlap();
static int comp_sim();
static int comp_grp_sim();
static char* getIndexedXPath(char *xpath);
void   reweightElement(VEC* dvec, VEC* evec, float lambda, float mu);
static int groupByArticles(ELEMENT_SIM* elementSims, int numElements);
static float getLogDocumentLength(VEC* vec);

int
init_tr_wt_elementlist (spec, unused)
SPEC *spec;
char *unused;
{
    CONTEXT old_context;

    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_tr_wt_elementlist");

    old_context = get_context();
    set_context (CTXT_DOC);

    if (VALID_FILE (vec_file)) {
		if (UNDEF == (vec_fd = open_vector (vec_file, vec_file_mode)))
			return (UNDEF);
    }

    if (UNDEF == (qid_vec_inst = init_qid_vec(spec, (char *) NULL)) ||
        UNDEF == (pp_inst = pp->init_proc (spec, "tr_elementlist.doc.")) ||
		UNDEF == (index_pp_inst = index_pp->init_proc (spec, "tr_elementlist.")) ||
        UNDEF == (vv_inst = init_sim_vec_vec(spec, NULL)) ||
        UNDEF == (elt_wt_proc_inst = elt_weight_vec->init_proc(spec, NULL))
		)
	return (UNDEF);

    if (sm_trace >= 4 &&
        UNDEF == (print_vec_inst = init_print_vec_dict(spec, NULL)))
        return UNDEF;

    set_context (old_context);

    max_elements = 20000;
    if (NULL == (element_sims = Malloc(max_elements, ELEMENT_SIM)))
        return UNDEF;

    PRINT_TRACE (2, print_string, "Trace: leaving init_tr_wt_elementlist");

    return (0);
}


int
tr_wt_elementlist (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
    int in_fd, tloc_fd, start_offset, status, i, j, n;
    float sim;
    FILE *out;
    EID eid;
    SM_INDEX_TEXTDOC doc, element; // element holds single element of doc
    SM_DISPLAY *sm_disp;
    VEC qvec, evec, dvec;
    VEC_PAIR vpair = {&qvec, &evec};
    TR_VEC tr_vec;
    char *cptr;
    xmlNodePtr xptr;
	long evecTfSum;

    PRINT_TRACE (2, print_string, "Trace: entering tr_wt_elementlist");

    /* Open input and output files */
    if (in_file == (char *) NULL) in_file = default_tr_file;
    if (NULL == out_file ||
        NULL == (out = fopen(out_file, "w")))
        out = stdout;
    if (UNDEF == (in_fd = open_tr_vec (in_file, tr_mode)) ||
        UNDEF == (tloc_fd = open_textloc (textloc_file, textloc_mode)))
	return (UNDEF);

    if (format < 2009) fprintf(out, HEADER_STRING, run_id);
    while (1 == read_tr_vec (in_fd, &tr_vec)) {
		if(tr_vec.qid < global_start) continue;
		if(tr_vec.qid > global_end) break;
    	qvec.id_num.id = tr_vec.qid; EXT_NONE(qvec.id_num.ext);
		if (UNDEF == qid_vec(&(qvec.id_num.id), &qvec, qid_vec_inst))
		    return(UNDEF);
        if (sm_trace >= 4) {
			fprintf(stdout, "Query vector:\n");
            if (UNDEF == print_vec_dict(&qvec, NULL, print_vec_inst))
	            return UNDEF;
        }

        num_elements = 0;
		for (i=0; i < tr_vec.num_tr; i++) {
            eid.id = tr_vec.tr[i].did;
            EXT_NONE(eid.ext);

			if (reweight_on_wholedoc) {
	     	   /* read the document vector from vec_file */
	        	dvec.id_num.id = tr_vec.tr[i].did;
			    EXT_NONE(dvec.id_num.ext);
		        if (1 != (status = seek_vector (vec_fd, &dvec)) ||
	    	        1 != (status = read_vector (vec_fd, &dvec))) {
		    		if (status == 0) continue;
					else return UNDEF;
		    	}
		   	 	PRINT_TRACE (4, print_vector, &dvec);
			}

            if (UNDEF == (status = pp->proc(&eid, &doc, pp_inst)))
                return UNDEF;
            if (0 == status) continue; // couldn't find document;

            if (sm_trace >= 4) {
                fprintf(stdout, 
                        "\n===========================================\n"
                        "Document %ld"
                        "\n===========================================\n",
                        tr_vec.tr[i].did);
            }

            /* element is a copy of the doc, except it has only 1 section */
            element = doc;
            element.mem_doc.num_sections = 1;            
            sm_disp = &(doc.mem_doc);

            start_offset = num_elements;
            /* compute similarity for each element of this document*/
            for (j = 0; j < sm_disp->num_sections; j++) {
#if 0
				// Ignore too small sections because the FOL table doesn't contain
				// mappings for elements having less than 50.
				if (element.textloc_doc.end_text - element.textloc_doc.begin_text + 1 <= 50)
					continue;
#endif
                element.mem_doc.sections = sm_disp->sections + j;

                if (sm_trace >= 4) {
                    xptr = (xmlNodePtr) element.mem_doc.sections->subsect;
                    cptr = xmlGetNodePath(xptr);
                    fprintf(stdout, "%s\n", cptr);
                    free(cptr);
                    print_int_textdoc_nohead(&element, NULL);
                    fprintf(stdout, "\n");
                }

                if (UNDEF == index_pp->proc(&element, &evec, index_pp_inst))
                    return UNDEF;

				// Calculate summation(term frequencies) using this nnn evec.
				if (docPriorSgn != 0)
					evecTfSum = getLogDocumentLength(&evec);

				/* Convert the returned element vector into the target weighting scheme (in place).
				 * (We are not using the tri_weight here). */
        		if (UNDEF == elt_weight_vec->proc(&evec, (VEC*) NULL, elt_wt_proc_inst))
                    return UNDEF;

				if (reweight_on_wholedoc)
					reweightElement(&dvec, &evec, lambda, mu);

                if (UNDEF == sim_vec_vec(&vpair, &sim, vv_inst))
                    return UNDEF;

				// Length normalization based on prior evec probability of relevance (based on length)
				sim += docPriorSgn * evecTfSum; 

                if (sm_trace >= 4) {
                    if (UNDEF == print_vec_dict(&evec, NULL, print_vec_inst))
                        return UNDEF;
                    printf("Similarity: %.4lf\n\n", sim);
                }
                
                if (num_elements == max_elements) {
                    max_elements = 2 * num_elements;
                    if (NULL == (element_sims = Realloc(element_sims, 
                                                        max_elements,
                                                        ELEMENT_SIM)))
                        return UNDEF;
                }
                element_sims[num_elements].section_num = j;
                element_sims[num_elements].sim = sim;
                element_sims[num_elements].num_chars = element.mem_doc.sections->end_section - element.mem_doc.sections->begin_section;
                element_sims[num_elements].start_offset = element.mem_doc.sections->begin_section;
                num_elements++;
            }

            /* Eliminate overlaps: if two elements are overlapping, keep
             * only the higher ranked one. Do this only for non-thorough task
             */
            if (UNDEF == eliminate_overlap(element_sims + start_offset,
                                           sm_disp, tr_vec.tr + i))
                return UNDEF;
        } /* end of processing elements for current query */

		if (*mode == 'r') {
			// For Relevant In Context Task, we need to group by articles
			if (UNDEF == groupByArticles(element_sims, num_elements))
				return UNDEF;
		}
		else {
	        qsort(element_sims, num_elements, sizeof(ELEMENT_SIM), comp_sim);
		}

		if (*mode == 't')
        	n = MIN(num_elements, num_wanted);
		else {
			// The flow has to pass through another invocation of smart convert text_inexfolsub or
			// text_inex_fcs_to_ric where we eliminate any remaining overlaps. Hence write out all
			// the elements in the output file lest in the final submission we might end up with too few.
			n = num_elements;
		}

		if (format < 2009) {
	        fprintf(out, "<topic topic-id=\"%ld\">\n", tr_vec.qid);
    	    for (j = 0; j < n; j++) {
        	    (void) fprintf (out, "  <result>\n"
                            "    <file>%s</file>\n"
                            "    <path>%s</path>\n"
                            "    <rsv>%.4f</rsv>\n"
                            "  </result>\n",
                            element_sims[j].filename,
                            element_sims[j].path,
                            element_sims[j].sim);
#if (VERBOSE > 0)
            fprintf(out, "  <original rank=%ld did=%ld/>\n",
                    element_sims[j].rank, element_sims[j].did);
#endif
        	}
	        (void) fprintf (out, "</topic>\n");
		}
		else {
			// TREC++ format
    	    for (j = 0; j < n; j++) {
            	fprintf (out, "%ld Q0 %s %d %f %s %s\n",
			         tr_vec.qid,
			         element_sims[j].filename,
			         j+1,
			         element_sims[j].sim,
			         run_id,
					 element_sims[j].path);
			}
		}

        for (i = 0; i < num_elements; i++) {
            free(element_sims[i].path);     // obtained via xmlGetNodePath
        }
    }
    if (format < 2009) fprintf(out, "</inex-submission>\n");

    if (UNDEF == close_tr_vec (in_fd))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving tr_wt_elementlist");
    return (1);
}

int
close_tr_wt_elementlist (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_tr_wt_elementlist");

    if (UNDEF == close_qid_vec(qid_vec_inst) ||
        UNDEF == pp->close_proc (pp_inst) ||
		UNDEF == index_pp->close_proc (index_pp_inst) ||
        UNDEF == close_sim_vec_vec(vv_inst) ||
        UNDEF == elt_weight_vec->close_proc(elt_wt_proc_inst))
		return (UNDEF);
    if (sm_trace >= 4 &&
        UNDEF == close_print_vec_dict(print_vec_inst))
        return UNDEF;

	if (UNDEF != vec_fd) {
		if (UNDEF == close_vector (vec_fd))
			return (UNDEF);
	}

    free(element_sims);

    PRINT_TRACE (2, print_string, "Trace: leaving close_tr_wt_elementlist");
    return (0);
}

static float getLogDocumentLength(VEC* vec)
{
	int i;
	float length = 0 ;
	for (i = 0; i < vec->num_conwt; i++) {
		length += vec->con_wtp[i].wt;
	}	
	return length == 0? 0 : log(length);
}

static int groupByArticles(ELEMENT_SIM* elementSims, int numElements)
{
	int i;
	ELEMENT_SIM  *thisElt, *foundEltSim;
	ENTRY        elt, *foundElt;

	hcreate(numElements);

	// Insert the keys 
	for (i = 0; i < numElements; i++) {
		thisElt = &elementSims[i];
		thisElt->grpSim = 0;
		elt.key = thisElt->filename;
		elt.data = (void*)thisElt;
		if (!hsearch(elt, ENTER))
			return UNDEF;
	}

	// Accumulate group score for elements belonging to the same article
	for (i = 0; i < numElements; i++) {
		thisElt = &elementSims[i];
		elt.key = thisElt->filename;
		elt.data = (void*)thisElt;
		foundElt = hsearch(elt, FIND);
		if (foundElt) {
			foundEltSim = (ELEMENT_SIM*)foundElt->data;
			foundEltSim->grpSim += thisElt->sim;	// add up the similarities
		}
	}
	
	// One more pass to write the accumulated grp sim value to all the member elements	
	for (i = 0; i < numElements; i++) {
		thisElt = &elementSims[i];
		elt.key = thisElt->filename;
		elt.data = (void*)thisElt;
		foundElt = hsearch(elt, FIND);
		if (foundElt) {
			foundEltSim = (ELEMENT_SIM*)foundElt->data;
			if (foundEltSim->grpSim > 0)
				thisElt->grpSim = foundEltSim->grpSim;
		}
	}

	// Sort the Element Sim objects by the grp similarity
    qsort(elementSims, numElements, sizeof(ELEMENT_SIM), comp_grp_sim);
	hdestroy();
}

static int compare_conwt(void* this, void* that)
{
	CON_WT *this_conwt, *that_conwt;
	this_conwt = (CON_WT*) this;
	that_conwt = (CON_WT*) that;

	return this_conwt->con < that_conwt->con ? -1 : this_conwt->con == that_conwt->con ? 0 : 1;
}

/* Warning: Assumes that dvec->con_wtp is sorted by con (i.e. term ids)
   The num_conwt of dvec should be greater than evec. Hence direct
   indexing from evec to dvec isn't possible. To locate the term from
   evec to dvec binary search is used. */  
void reweightElement(VEC* dvec, VEC* evec, float lambda, float mu) {
	CON_WT *conwt, *dvec_conwt, *dsubvec_conwtp, *esubvec_conwtp;
	long ctype;

	for (ctype = 0, dsubvec_conwtp = dvec->con_wtp, esubvec_conwtp = evec->con_wtp; ctype < evec->num_ctype; ctype++) {
		for (conwt = esubvec_conwtp; conwt < esubvec_conwtp + evec->ctype_len[ctype]; conwt++) {
			dvec_conwt = bsearch(conwt, dsubvec_conwtp, dvec->ctype_len[ctype], sizeof(CON_WT), compare_conwt);
			if (!dvec_conwt) {
				conwt->wt = log (1 + mu/(1-mu)*conwt->wt);
			}
			else {
				conwt->wt = log (1 + mu/(1-lambda-mu)*conwt->wt + lambda/(1-lambda-mu)*dvec_conwt->wt);
			}
		}
		dsubvec_conwtp += dvec->ctype_len[ctype]; 
		esubvec_conwtp += evec->ctype_len[ctype]; 
	}
}

/* Checks if the path points to a text node and if so check if the paths
 * are same. In that case we would remove one of them
*/
static int checkTextNode(SM_DISP_SEC *this, SM_DISP_SEC *that)
{
	char *thispath, *thatpath;
	char *p, *q;
	int  sameNode = 0;

	do {
		thispath = xmlGetNodePath(this->subsect);
		thatpath = xmlGetNodePath(that->subsect);

		p = rindex(thispath, '[');
		q = rindex(thatpath, '[');
		if (!p || !q)
			break;
		p = p-7; q = q-7;  // "/text()["
		if (strncmp(p+1, "text", 4) || strncmp(q+1, "text", 4))
			break;
		if (p < thispath || q < thatpath)
			break;
		if (p - thispath != q - thatpath)
			break;	// no chance of being equal if the lengths don't match
		if (strncmp(thispath, thatpath, p - thispath))
			break;
		sameNode = 1;
	}
	while (0);

	free(thispath);
	free(thatpath);
	return sameNode;
}

static int
eliminate_overlap(elements, sm_disp, tr)
ELEMENT_SIM *elements;
SM_DISPLAY *sm_disp;
TR_TUP *tr;
{
    char *cptr, *fname, *extension;
    int i, j, n = (element_sims + num_elements) - elements;
    SM_DISP_SEC *this, *that;
    xmlNodePtr xptr;
	int thisContainedInThat, thatContainedInThis;

    qsort(elements, n, sizeof(ELEMENT_SIM), comp_sim);
#if (VERBOSE == 3)
    fprintf(stdout, 
            "\n============================================================\n"
            "Document %ld(%s): before elimination of duplicates"
            "\n============================================================\n",
            tr->did, sm_disp->file_name);
    for (i = 0; i < n; i++) {
        this = sm_disp->sections + elements[i].section_num;
        xptr = (xmlNodePtr) this->subsect;
        cptr = xmlGetNodePath(xptr);
        fprintf(stdout, "%ld: %s\n%.6f\n",
                elements[i].section_num, cptr, elements[i].sim);
        free(cptr);
    }
#endif

    for (i = 0; i < n; i++) {
		if (elements[i].sim < 0) continue;
        this = sm_disp->sections + elements[i].section_num;
        for (j = i+1; j < n; j++) {
			if (elements[j].sim < 0) continue;
            that = sm_disp->sections + elements[j].section_num;
			thisContainedInThat = this->begin_section >= that->begin_section && this->end_section <= that->end_section;
			thatContainedInThis = this->begin_section <= that->begin_section && this->end_section >= that->end_section;

            /* Mark an overlapping section by making sim negative. */
			if (thisContainedInThat || thatContainedInThis) {
            	elements[j].sim = -1;
			}
        }
    }

    /* eliminate overlapping sections */
    qsort(elements, n, sizeof(ELEMENT_SIM), comp_sim);
    i = n-1;
    while (i >= 0 && elements[i].sim < EPSILON) {
        n--;
        i--;
    }

    /* get the file name and then strip the .xml extension (INEX format) */
    if (NULL == (cptr = rindex(sm_disp->file_name, '/')))
        cptr = sm_disp->file_name;
    else cptr++;
    if (NULL == (fname = strdup(cptr)))
        return UNDEF;
    if (NULL != (extension = rindex(fname, '.')))
        *extension = '\0';

    /* fill in other fields for remaining sections */
    while (i >= 0) {
        strncpy(elements[i].filename, fname, PATH_LEN/4 - 1);
        elements[i].filename[PATH_LEN/4 - 1] = '\0';
        /* subsection pointer (subsect) points to XML tree node  
         * (see (9) in documentation above) 
         */
        this = sm_disp->sections + elements[i].section_num;
        xptr = (xmlNodePtr) this->subsect;
        elements[i].path = getIndexedXPath(xmlGetNodePath(xptr));
#if (VERBOSE > 0)
        elements[i].did = tr->did;
        elements[i].rank = tr->rank;
#endif
        i--;
    }

#if (VERBOSE == 3)
    fprintf(stdout, 
            "\n============================================================\n"
            "Document %ld(%s): after elimination of duplicates"
            "\n============================================================\n",
            tr->did, sm_disp->file_name);
    for (i = 0; i < n; i++) {
        fprintf(stdout, "%ld: %s\n%.6f\n",
                elements[i].section_num, elements[i].path, elements[i].sim);
    }
#endif

    num_elements = (elements - element_sims) + n;
    return 1;
}


/* Postprocess the path string to append '[1]' after every node name if not already indexed */
static char* getIndexedXPath(char *xpath) {
	int pathlen = strlen(xpath);
	int newpathlen = (pathlen + 1)<<1;
	char *newxpath = (char*) malloc(newpathlen);    // twice the size to be safe (?)
	char *p = xpath, *q = newxpath;
	*q = *p;
	p++; q++;

	while (*p) {
		if (*p == '/' && *(p-1) != ']') {
			*q++ = '[';
			*q++ = '1';
			*q++ = ']';
		}   
		*q++ = *p++;
	}

	// Handle the last node
	if (*(q-1) != ']') {
		*q++ = '[';
		*q++ = '1';
		*q++ = ']';
	}
	*q = NULL;
	return newxpath;
}

static int 
comp_sim (e1, e2)
const void *e1;
const void *e2;
{
    ELEMENT_SIM *es1 = (ELEMENT_SIM *) e1;
    ELEMENT_SIM *es2 = (ELEMENT_SIM *) e2;

    if (es1->sim > es2->sim) return -1;
    if (es1->sim < es2->sim) return 1;
	if (es1->num_chars < es2->num_chars) return -1;
	if (es1->num_chars > es2->num_chars) return 1;
    return 0;
}

static int 
comp_grp_sim (e1, e2)
const void *e1;
const void *e2;
{
	long el1_did, el2_did;
    ELEMENT_SIM *es1 = (ELEMENT_SIM *) e1;
    ELEMENT_SIM *es2 = (ELEMENT_SIM *) e2;

    if (es1->grpSim > es2->grpSim) return -1;
    if (es1->grpSim < es2->grpSim) return 1;

	el1_did = atol(es1->filename);
	el2_did = atol(es2->filename);

	if (el1_did < el2_did) return -1;
	if (el1_did > el2_did) return 1;
	
	return comp_sim(e1, e2);
}
