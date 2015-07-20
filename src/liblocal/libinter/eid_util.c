#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/eid_util.c,v 11.2 1993/02/03 16:49:43 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include <ctype.h>
#include "common.h"
#include "context.h"
#include "docindex.h"
#include "local_eid.h"
#include "functions.h"
#include "inst.h"
#include "inter.h"
#include "local_inter.h"
#include "param.h"
#include "proc.h"
#include "sm_display.h"
#include "smart_error.h"
#include "spec.h"
#include "textloc.h"
#include "trace.h"
#include "vector.h"

static char *textloc_file;
static long textloc_mode;
static PROC_TAB *index_pp;

static SPEC_PARAM spec_args[] = {
    {"local.inter.eid.textloc_file", getspec_dbfile,
       (char *) &textloc_file},
    {"local.inter.eid.textloc_file.rmode", getspec_filemode,
       (char *) &textloc_mode},
    PARAM_FUNC( "local.inter.index_pp", &index_pp ),
    TRACE_PARAM ("local.inter.eid.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

typedef struct {
    /* bookkeeping */
    int valid_info;

    int textdoc_inst;

    PROC_TAB *index_pp;
    int index_pp_inst[NUM_PTYPES];
    VEC vec;

    int max_eids;
    EID_LIST elist;
    int last_did;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

extern int init_get_textdoc(), get_textdoc(), close_get_textdoc();
static int extract_eids _AP((char *, STATIC_INFO *));
static int add_eid _AP((EID, STATIC_INFO *));


int
init_eid_util (spec, prefix)
SPEC *spec;
char *prefix;
{
    char new_prefix[PATH_LEN];
    STATIC_INFO *ip;
    int new_inst;
    CONTEXT old_context;

    if(UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
      return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_eid_util");

    NEW_INST( new_inst );
    if(UNDEF == new_inst) return UNDEF;
    ip = &info[new_inst];
    ip->valid_info = 1;

    if(UNDEF == (ip->textdoc_inst = init_get_textdoc(spec, prefix)))
      return UNDEF;

    ip->index_pp = index_pp;
    old_context = get_context();
    add_context (CTXT_DOC);
    sprintf(new_prefix, "%sdoc.", (prefix == NULL) ? "" : prefix);
    if(UNDEF ==	
       (ip->index_pp_inst[ENTIRE_DOC] = ip->index_pp->init_proc(spec, new_prefix)))
      return UNDEF;

    add_context (CTXT_SECT);
    sprintf(new_prefix, "%ssect.", (prefix == NULL) ? "" : prefix);
    if(UNDEF == 
       (ip->index_pp_inst[P_SECT] = ip->index_pp->init_proc(spec, new_prefix)))
      return UNDEF;
    del_context (CTXT_SECT);
    add_context (CTXT_PARA);
    sprintf(new_prefix, "%spara.", (prefix == NULL) ? "" : prefix);
    if(UNDEF == 
       (ip->index_pp_inst[P_PARA] = ip->index_pp->init_proc(spec, new_prefix)))
      return UNDEF;
    del_context (CTXT_PARA);
    add_context (CTXT_SENT);
    sprintf(new_prefix, "%ssent.", (prefix == NULL) ? "" : prefix);
    if(UNDEF == 
       (ip->index_pp_inst[P_SENT] = ip->index_pp->init_proc(spec, new_prefix)))
      return UNDEF;
    del_context (CTXT_SENT);
    set_context (old_context);

    ip->max_eids = 1;
    if (NULL == (ip->elist.eid = Malloc(ip->max_eids, EID)))
	return(UNDEF);
    ip->last_did = UNDEF; 
    ip->vec.num_conwt = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving init_eid_util");
    return new_inst;
}  


int
close_eid_util (inst)
int inst;
{
    int i;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_eid_util");
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_eid_util");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (UNDEF == close_get_textdoc(ip->textdoc_inst))
	return UNDEF;
    for(i = 0; i < NUM_PTYPES; i++)
	if(UNDEF == ip->index_pp->close_proc( ip->index_pp_inst[i] ))
	    return UNDEF;
    
    Free(ip->elist.eid);
    ip->valid_info = 0;
    PRINT_TRACE (2, print_string, "Trace: leaving close_eid_util");

    return (0);
}


int 
eid_textdoc (eid, pp, inst)
EID eid;
SM_INDEX_TEXTDOC *pp;
int inst;
{
    int para_count, sent_count, i, j = 0, ind;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering eid_textdoc");
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "eid_textdoc");
        return (UNDEF);
    }
    ip  = &info[inst];
    
    if (UNDEF == get_textdoc(eid.id, pp, ip->textdoc_inst))
	return(UNDEF);
    pp->mem_doc.id_num = eid;
    if (eid.ext.type == ENTIRE_DOC) return(1);

    if (eid.ext.type == P_SECT) {
	assert((long) eid.ext.num < pp->mem_doc.num_sections);
	pp->mem_doc.num_sections = 1;
	pp->mem_doc.sections = &pp->mem_doc.sections[eid.ext.num];
    }

    if (eid.ext.type == P_PARA) {
	para_count = 0;
	for (i = 0; i < pp->mem_doc.num_sections; i++)
	    if ((long) eid.ext.num < (para_count += 
			       pp->mem_doc.sections[i].num_subsects))
		break;
	assert((long) eid.ext.num < para_count);
	/* find para no. in current section */
	ind = eid.ext.num - para_count + pp->mem_doc.sections[i].num_subsects;
	pp->mem_doc.num_sections = 1;
	pp->mem_doc.sections = &(pp->mem_doc.sections[i].subsect[ind]);
    }

    if (eid.ext.type == P_SENT) {
	sent_count = 0;
	for (i = 0; i < pp->mem_doc.num_sections; i++) {
	    for (j = 0; j < pp->mem_doc.sections[i].num_subsects; j++) {
		sent_count += pp->mem_doc.sections[i].subsect[j].num_subsects;
		if((long) eid.ext.num < sent_count)
		    break;
	    }
	    if((long) eid.ext.num < sent_count) break;
	}

	assert((long) eid.ext.num < sent_count);
	/* find sent no. in current para */
	ind = eid.ext.num - sent_count + 
	    pp->mem_doc.sections[i].subsect[j].num_subsects;
	pp->mem_doc.num_sections = 1;
	pp->mem_doc.sections
	    = &(pp->mem_doc.sections[i].subsect[j].subsect[ind]);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving eid_textdoc");
    return (1);
}


int 
eid_vec(eid, vec, inst)
EID eid;
VEC *vec;
int inst;
{
    STATIC_INFO *ip;
    SM_INDEX_TEXTDOC pp;
    int indexing;

    PRINT_TRACE (2, print_string, "Trace: entering eid_vec");
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "eid_vec");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (UNDEF == eid_textdoc(eid, &pp, inst))
	return(UNDEF);
    
    indexing = pp.mem_doc.id_num.ext.type;
    if (UNDEF == ip->index_pp->proc(&pp, &ip->vec,
				    ip->index_pp_inst[indexing]))
	return UNDEF;
    ip->vec.id_num = eid;
    *vec = ip->vec;

    PRINT_TRACE (2, print_string, "Trace: leaving eid_vec");
    
    return 1;
}


int 
inter_get_parts_vec(eidstring, vec, inst)
char *eidstring;
VEC *vec;
int inst;
{
    EID_LIST elist;
    STRING_LIST strlist;

    PRINT_TRACE (2, print_string, "Trace: entering inter_get_parts_vec");

    strlist.string = &eidstring;
    strlist.num_strings = 1;
    if (UNDEF == stringlist_to_eidlist(&strlist, &elist, inst) ||
	UNDEF == eid_vec(elist.eid[0], vec, inst))
	return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving inter_get_parts_vec");
    return 1;
}


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Convert an eid to a string
 *2 eid_to_string(eid,sptr)
 *3   EID *eid;
 *3   char **sptr;

 *7 Convert the passed eid into an appropriate string.  
 *8 The returned string is NOT malloc'd, so it should not be freed.
 *8 However, this routine maintains several output buffers and rotates
 *8 between them, so a few calls in a row shouldn't overwrite each other.
***********************************************************************/
int 
eid_to_string( eid, sptr )
EID eid;
char **sptr;
{
    static char buf[5][PATH_LEN];
    static which_buf = 0;
    char *ptr = buf[which_buf];

    (void) sprintf( ptr, "%ld", eid.id );
    while (*ptr) ptr++;

    switch (eid.ext.type) {
      case ENTIRE_DOC:
	break;
      case P_SECT:
	(void) sprintf(ptr, ".c%d", eid.ext.num);
	break;
      case P_PARA:
	(void) sprintf(ptr, ".p%d", eid.ext.num);
	break;
      case P_SENT:
	(void) sprintf(ptr, ".s%d", eid.ext.num);
	break;
    }

    *sptr = buf[which_buf];
    which_buf = (which_buf + 1) % 5;
    return 1;
}


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Convert the interactive command-line to a list of eids.
 *2 stringlist_to_eidlist ( is, eidlist, inst )
 *3 INTER_STATE *is;
 *3 EID_LIST *eidlist;
 *3 int inst;

 *7 As a feature, we propagate the doc number if not given after 
 *7 the first time (so "100.c3 .c2" is the same as "100.c3 100.c2").
 *7 Also, we handle did ranges like 100.c4-9.
***********************************************************************/
int
stringlist_to_eidlist(strlist, eidlist, inst)
STRING_LIST *strlist;
EID_LIST *eidlist;
int inst;
{
    char tmp_buf[PATH_LEN], saved_buf[PATH_LEN], *dash, *cptr;
    int start, finish, i, j;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering stringlist_to_eidlist");
    if (!VALID_INST(inst)) {
	set_error (SM_ILLPA_ERR, "Instantiation", "stringlist_to_eidlist");
	return (UNDEF);
    }
    ip  = &info[inst];

    ip->elist.num_eids = 0; 
    for (i=0; i < strlist->num_strings; i++) {

	/* propagate docid if necessary */
	cptr = strlist->string[i];
	if ((!isdigit(*cptr) && *cptr != '.') ||
	    (*cptr == '.' && ip->last_did == UNDEF)) { 
	    set_error(SM_ILLPA_ERR, "Bad argument", "stringlist_to_eidlist");
	    return(UNDEF);
	}
        if (*cptr == '.' )
	    (void) sprintf(saved_buf, "%d%s", ip->last_did, cptr);
	else
	    (void) sprintf(saved_buf, "%s", cptr);
    
	/* handle ranges */
	if (NULL == (dash = strchr(saved_buf, '-' ))) {
	    if (UNDEF == extract_eids(saved_buf, ip)) {
		set_error(SM_ILLPA_ERR, "Bad argument", "stringlist_to_eidlist");
		return(UNDEF);
	    }
	}
	else {
	    cptr = dash - 1;
	    while (cptr > saved_buf && isdigit(*(cptr-1))) cptr--;
	    start = atol(cptr);
	    finish = atol(dash+1);
	    *cptr = '\0';  /* everything from here we replace */

	    if (finish < start) {
		set_error(SM_ILLPA_ERR, "Bad range in docid", 
			  "stringlist_to_eidlist");
		return UNDEF;
	    }

	    for (j = start; j <= finish; j++) {
		(void) sprintf(tmp_buf, "%s%d", saved_buf, j);
		if (UNDEF == extract_eids(tmp_buf, ip))
		    return(UNDEF);
	    }
	}
    }

    *eidlist = ip->elist;
    PRINT_TRACE (2, print_string, "Trace: leaving stringlist_to_eidlist");
    return(1);
}


#define P_UNDEF -1
#define P_WILD -2

typedef int PART_LIST[NUM_PTYPES];

static int string_to_partlist _AP((char *, PART_LIST));


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Get the eids corresponding to a processed command-line argument
 *2 extract_eids( str, ip )
 *3   char *str;
 *3   STATIC_INFO *ip;

 *7 The passed string corresponds to a set of EIDs (> 1 if any part is
 *7 wildcarded). These EIDs are added to the current list of EIDs.
***********************************************************************/
int
extract_eids(str, ip)
char *str;
STATIC_INFO *ip;
{
    int num_paras_so_far, num_sents_so_far, para_count, sent_count, ind;
    int i, j, k;
    PART_LIST plist;
    EID tmp_eid;
    SM_DISP_SEC tmp_para;
    SM_INDEX_TEXTDOC pp;
    
    PRINT_TRACE (2, print_string, "Trace: entering extract_eids");

    if (1 != string_to_partlist(str, plist))
	return(UNDEF);
    tmp_eid.id = ip->last_did = plist[ENTIRE_DOC];
    EXT_NONE(tmp_eid.ext); 

    if (plist[P_SECT] == P_UNDEF && plist[P_PARA] == P_UNDEF && 
	plist[P_SENT] == P_UNDEF) {
	if (UNDEF == add_eid(tmp_eid, ip)) return(UNDEF);
	return(1);
    }

    if (UNDEF == get_textdoc(ip->last_did, &pp, ip->textdoc_inst))
	return(UNDEF);

    num_paras_so_far = num_sents_so_far = 0;

    if (plist[P_SECT] == P_WILD) { 
	tmp_eid.ext.type = P_SECT;
	for (i = 0; i < pp.mem_doc.num_sections; i++) { 
	    tmp_eid.ext.num = i;
	    if (UNDEF == add_eid(tmp_eid, ip))
		return(UNDEF);
	}
	return(1);
    }

    if (plist[P_SECT] >= 0) { 
	tmp_eid.ext.type = P_SECT;
	tmp_eid.ext.num = plist[P_SECT];

	if ((long) tmp_eid.ext.num >= pp.mem_doc.num_sections) {
	    set_error(SM_ILLPA_ERR, "Not that many sections", "extract_eids");
	    return(UNDEF);
	}
	if (plist[P_PARA] == P_UNDEF && plist[P_SENT] == P_UNDEF) {
	    if (UNDEF == add_eid(tmp_eid, ip)) return(UNDEF);
	    return(1);
	}

	/* get the para/sent no for 1st para/sent in this section/para */
	for (i = 0; i < plist[P_SECT]; i++) {
	    num_paras_so_far += pp.mem_doc.sections[i].num_subsects;
	    if (plist[P_SENT] != P_UNDEF)
		for (j = 0; j < pp.mem_doc.sections[i].num_subsects; j++)
		    num_sents_so_far += 
			pp.mem_doc.sections[i].subsect[j].num_subsects;
	}

	/* get the appropriate sub-tree */
	pp.mem_doc.num_sections = 1;
	pp.mem_doc.sections = &pp.mem_doc.sections[plist[P_SECT]];
    }

    if (plist[P_PARA] == P_WILD) { 
	tmp_eid.ext.type = P_PARA;
	for (i = 0; i < pp.mem_doc.num_sections; i++)
	    for (j = 0; j < pp.mem_doc.sections[i].num_subsects; j++) { 
		tmp_eid.ext.num = num_paras_so_far++;
		if (UNDEF == add_eid(tmp_eid, ip))
		    return(UNDEF);
	    }
	return(1);
    }

    if (plist[P_PARA] >= 0) { 
	tmp_eid.ext.type = P_PARA;
	tmp_eid.ext.num = num_paras_so_far + plist[P_PARA];

	para_count = 0;
	for (i = 0; i < pp.mem_doc.num_sections; i++) {
	    para_count += pp.mem_doc.sections[i].num_subsects;
	    if (plist[P_PARA] < para_count) break;
	    for (j = 0; j < pp.mem_doc.sections[i].num_subsects; j++)
		num_sents_so_far += 
		    pp.mem_doc.sections[i].subsect[j].num_subsects;
	}
	if (plist[P_PARA] >= para_count) {
	    set_error(SM_ILLPA_ERR, "Not that many paras", "extract_eids");
	    return(UNDEF);
	}
	if (plist[P_SENT] == P_UNDEF) {
	    if (UNDEF == add_eid(tmp_eid, ip)) return(UNDEF);
	    return(1);
	}

	/* get para no within sect and get sent no for 1st sent in this para */
	ind = plist[P_PARA]-para_count + pp.mem_doc.sections[i].num_subsects;
	for (j = 0; j < ind; j++)
	    num_sents_so_far += 
		pp.mem_doc.sections[i].subsect[j].num_subsects;

	/* get the appropriate subtree */
	/* NB: The subtree starting at the para level is located in 
	 * get_textdoc's data structures and we do not want to overwrite 
	 * them. Hence, the copying.
	 */
	pp.mem_doc.num_sections = 1;
	tmp_para = pp.mem_doc.sections[i];
	tmp_para.num_subsects = 1;
	tmp_para.subsect = &pp.mem_doc.sections[i].subsect[ind];
	pp.mem_doc.sections = &tmp_para;
    }

    tmp_eid.ext.type = P_SENT;
    if (plist[P_SENT] == P_WILD) { 
	for (i = 0; i < pp.mem_doc.num_sections; i++) 
	    for (j = 0; j < pp.mem_doc.sections[i].num_subsects; j++)
                for (k=0; k < pp.mem_doc.sections[i].subsect[j].num_subsects;
                     k++) { 
                    tmp_eid.ext.num = num_sents_so_far++;
                    if (UNDEF == add_eid(tmp_eid, ip))
                        return(UNDEF);
                }
	return(1);
    }

    tmp_eid.ext.num = num_sents_so_far + plist[P_SENT];
	
    sent_count = 0;
    for (i = 0; i < pp.mem_doc.num_sections; i++) 
	for (j = 0; j < pp.mem_doc.sections[i].num_subsects; j++)
	    sent_count += pp.mem_doc.sections[i].subsect[j].num_subsects;
    if (plist[P_SENT] >= sent_count) {
	set_error(SM_ILLPA_ERR, "Not that many sents", "extract_eids");
	return(UNDEF);
    }
    if (UNDEF == add_eid(tmp_eid, ip)) return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving extract_eids");    
    return(1);
}


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Convert a docid string to a part list
 *1 local.inter.parts.string_plist
 *2 string_to_partlist ( string, parts )
 *3   char *string;
 *3   PART_LIST parts;

 *7 Parse a user-supplied docid with possible sentence, paragraph,
 *7 or section ids.  Input has the form:
 *7	[0-9]*  [ .CcPpSs[0-9]* ]...
 *7 Where '100.c8.p3' means the 3rd paragraph of section 8 of document
 *7 100.
 *7 Normalizes the partlist, so that only the smallest wildcarded part 
 *7 remains wildcarded, by converting the coarser wildcards to undefs.
***********************************************************************/
static int 
string_to_partlist (ptr, parts)
char *ptr;
PART_LIST parts;
{
    int i;

    for (i=0; i<NUM_PTYPES; i++)
	parts[i] = P_UNDEF;

    if (ptr == NULL)
        return (0);

    parts[ENTIRE_DOC] = atol (ptr);
    while (*ptr && isdigit (*ptr))
	ptr++;

    while (*ptr == '.') {
        ptr++;
        switch (*ptr) {
          case 'C':
          case 'c':
            /* Found section */
            ptr++;
	    if (! *ptr || !isdigit(*ptr)) {
		parts[P_SECT] = P_WILD;
		if (*ptr && *ptr=='*' ) ptr++;
	    }
	    else {
		parts[P_SECT] = atol (ptr);
		while (*ptr && isdigit (*ptr))
		    ptr++;
	    }
            break;

          case 'P':
          case 'p':
            /* Found paragraph */
            ptr++;
	    if (! *ptr || !isdigit(*ptr)) {
		parts[P_PARA] = P_WILD;
		if (*ptr && *ptr=='*' ) ptr++;
		if (parts[P_SECT] == P_WILD) parts[P_SECT] = P_UNDEF;

	    }
	    else {
		parts[P_PARA] = atol (ptr);
		while (*ptr && isdigit (*ptr))
		    ptr++;
	    }
            break;

          case 'S':
          case 's':
            /* Found sentence */
            ptr++;
	    if (! *ptr || !isdigit(*ptr)) {
		parts[P_SENT] = P_WILD;
		if (*ptr && *ptr=='*' ) ptr++;
		if (parts[P_SECT] == P_WILD) parts[P_SECT] = P_UNDEF;
		if (parts[P_PARA] == P_WILD) parts[P_PARA] = P_UNDEF;
	    }
	    else {
		parts[P_SENT] = atol (ptr);
		while (*ptr && isdigit (*ptr))
		    ptr++;
	    }
            break;

          default:
            /* Illegal specifier.  Just return */
            return (0);
        }
    }
    return (1);
}


static int
add_eid(eid, ip)
EID eid;
STATIC_INFO *ip;
{
    if (ip->elist.num_eids == ip->max_eids) {
	ip->max_eids *= 2;
	if (NULL == (ip->elist.eid = Realloc(ip->elist.eid, ip->max_eids, 
					     EID)))
	    return(UNDEF);
    }
    ip->elist.eid[ip->elist.num_eids++] = eid;
    return(1);
}
