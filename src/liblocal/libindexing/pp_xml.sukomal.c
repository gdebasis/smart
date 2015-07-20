#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/pp_xml.c,v 11.2 1993/02/03 16:52:04 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 pre-parser back-end that preparses XML file
 *1 local.index.preparse.preparser.xml
 *2 pp_xml (sm_buf, output_doc, inst)
 *3   SM_BUF *sm_buf;
 *3   SM_INDEX_TEXTDOC *output_doc;
 *3   int inst;
 *4 init_pp_xml (spec_ptr, qd_prefix)
 *4 close_pp_xml (inst)
 *5    "index.preparse.trace"
 *5    "index.preparse.hierarchical"
 *5    "*index.num_pp_sections"
 *5    "*index.pp.default_section_name"
 *5    "*index.pp.default_section_action"
 *5    "*pp_section.*.string"
 *5    "*pp_section.*.section_name"
 *5    "*pp_section.*.action"
 *5    "*pp_section.*.oneline_flag"
 *5    "*pp_section.*.newdoc_flag"

 *7 Depending on whether index.preparse.hierarchical is false or true,
 *7 generates a flat SM_INDEX_TEXTDOC (1 level of sections), 
 *7 or a hierarchical structure (sections, sub-sections, sub-sub-sections,
 *7 etc.) reflecting the structure of the doc.

 *7 Returns 1 if text preparsed, 0 if no text, UNDEF if error.

 *8 Since the XML parser does not give the byte offset of the text in
 *8 the current node, we copy all useful text into ip->buf, and setup
 *8 the offsets in sm_disp into that buffer. 
 *8 To make sure that the content of ip->buf is used by the subsequent 
 *8 routines (e.g. parser), we set sm_disp->doc_text to point to ip->buf
 *8 instead of the original document.
 *8 (see index_coll.c -> pp_gen.c:267 (pp_next_text) -> get_next_text.c:214)

 *8 Some preparsing utility routines copied from pp_gen_line.c (see end).

 *9 Assumes that each input (.xml) file corresponds to a single document.
 *9 Thus, sm_buf->end is never adjusted.

 *9 NOTE!! NOTE!! NOTE!!
 *9 1. create_sections_hier needs to be cleaned up: SUPPLEMENTARY, and
 *9    ip->nodelist may not be necessary.
 *9 2. Dirty hack: subsection pointer (subsect) made to point to XPath
 *9    for the section. 
 *9    (This may also result in leaked memory, since the path must be
 *9    freed by us after use.)
***********************************************************************/

#include "common.h"
#include "param.h"
#include "spec.h"
#include "buf.h"
#include "trace.h"
#include "smart_error.h"
#include "preparser.h"
#include "sm_display.h"
#include "functions.h"
#include "inst.h"
#include <libxml/parser.h>
#include <libxml/tree.h>

static int hier;
static SPEC_PARAM pp[] = {
    {"index.preparse.hierarchical", getspec_bool, (char *) &hier},
    TRACE_PARAM ("index.preparse.trace")
};
static int num_pp = sizeof(pp) / sizeof(pp[0]);

static char *prefix;
static long num_pp_sections;
static char *default_section_name;
static char *default_section_action;
static SPEC_PARAM_PREFIX pp_pre[] = {
  {&prefix, "index.num_pp_sections", getspec_long, (char *) &num_pp_sections},
  {&prefix, "index.pp.default_section_name", getspec_string, (char *) &default_section_name},
  {&prefix, "index.pp.default_section_action",getspec_string, (char *) &default_section_action},
};
static int num_pp_pre = sizeof(pp_pre) / sizeof(pp_pre[0]);

static char *sect_prefix;
static char *string;
static char *section_name;
static char *action;
static int  oneline_flag;
static int  newdoc_flag;
static SPEC_PARAM_PREFIX sect_spec_args[] = {
    {&sect_prefix, "string",       getspec_intstring, (char *) &string},
    {&sect_prefix, "section_name", getspec_string,    (char *) &section_name},
    {&sect_prefix, "action",       getspec_string,    (char *) &action},
    {&sect_prefix, "oneline_flag", getspec_bool,      (char *) &oneline_flag},
    {&sect_prefix, "newdoc_flag",  getspec_bool,      (char *) &newdoc_flag},
};
static int num_sect_spec_args = sizeof(sect_spec_args) /
				sizeof(sect_spec_args[0]);

/* Increment to increase size of section storage by */
#define DISP_SEC_SIZE 60

static int init_pp_sections(), init_lookup_section_start(), 
    lookup_section_start(), pp_put_section(), comp_pp_section();
static int create_sections_hier(), create_sections_nonhier();
static int fix_sections();
static int count_nodes();
static int pp_copy_nw();

/* Supplementary info to be kept at each node of the XML tree */
typedef struct {
    int begin, end;
    xmlNodePtr node; 	
} SUPPLEMENTARY;

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;
    char *qd_prefix;             /* Prefix for all spec params, normally
				    either "doc." or "query." */

    PP_INFO *pp_info;            /* Preparse table of actions */
    unsigned char lookup_table[256]; /* lookup_table[c] is index pointing to
                                    pp_info->section which starts with
                                    char c.  If lookup_table[c] == num_sections
				    then there are no such sections */
    unsigned char count_lookup_table[256]; /* count_lookup_table[c] is count
				    of sections which start with char c */

    SM_DISP_SEC *mem_disp;        /* Section occurrence info for doc giving
                                     byte offsets within buf */
    int num_sections;             /* Number of sections seen in doc */
    unsigned max_num_sections;    /* max current storage reserved for
                                     mem_disp */

    SM_BUF buf;
    int curr_location;           /* Points to the character location within 
                                  * ip->buf that is currently being 
                                  * processed */

    xmlNodePtr *stack;            /* private stack (used to avoid recursion
                                  * during tree traversal) */
    xmlNodePtr *nodelist;         /* list of "interesting" nodes */
    SUPPLEMENTARY *supplist;     /* supplementary info for each interesting node */
    int stack_size, max_nodes, num_nodes;

    int (*create_sections)();    /* Points to create_sections_hier() or
                                    create_sections_nonh(). */
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;


int
init_pp_xml (spec, qd_prefix)
SPEC *spec;
char *qd_prefix;
{
    STATIC_INFO *ip;
    int i, new_inst;

    NEW_INST (new_inst);
    if (UNDEF == new_inst) return (UNDEF);
    ip = &info[new_inst];

    if (UNDEF == lookup_spec(spec, &pp[0], num_pp))
	return(UNDEF);

    prefix = qd_prefix;
    if (UNDEF == lookup_spec_prefix (spec, &pp_pre[0], num_pp_pre))
	return(UNDEF);

    PRINT_TRACE(2, print_string, "Trace: entering init_pp_xml");
    PRINT_TRACE(6, print_string, qd_prefix);

    ip->qd_prefix = qd_prefix;

    /* Initialize the pp_info structure which describes the actions to
       be taken by the preparser */
    if (NULL == (ip->pp_info = (PP_INFO *) malloc(sizeof(PP_INFO))))
        return(UNDEF);
    ip->pp_info->doc_type = 0;
    if (UNDEF == init_pp_sections(spec, ip->pp_info, qd_prefix))
	return(UNDEF);
    
    ip->max_num_sections = DISP_SEC_SIZE;
    if (NULL == (ip->mem_disp = Malloc(DISP_SEC_SIZE, SM_DISP_SEC)))
        return (UNDEF);

    if (UNDEF == init_lookup_section_start(ip->pp_info->pp_sections,
                                           ip->pp_info->num_pp_sections,
                                           ip))
        return (UNDEF);

    ip->buf.size = ip->buf.end = 0;
    ip->buf.buf = (unsigned char *) NULL;

    ip->stack_size = 100;
    ip->max_nodes = 50;
    if (NULL == (ip->stack = Malloc(ip->stack_size, xmlNode *)) ||
        NULL == (ip->nodelist = Malloc(ip->max_nodes, xmlNode *)) ||
        NULL == (ip->supplist = Malloc(ip->max_nodes, SUPPLEMENTARY)))
        return (UNDEF);

    for (i=0; i < ip->max_nodes; i++) {
	ip->supplist[i].begin = 0;
	ip->supplist[i].end = 0;
    }

    if (hier) ip->create_sections = create_sections_hier;
    else ip->create_sections = create_sections_nonhier;

    LIBXML_TEST_VERSION ; // initialize libxml2 library

    PRINT_TRACE(2, print_string, "Trace: leaving init_pp_xml");

    ip->valid_info = 1;
    return (new_inst);
}

int
pp_xml (sm_buf, output_doc, inst)
SM_BUF *sm_buf;
SM_INDEX_TEXTDOC *output_doc;
int inst;
{
    SM_DISPLAY *sm_disp = &(output_doc->mem_doc);
    STATIC_INFO *ip;

    PP_SECTIONS *current_secptr;

    /*** beginning of Sukomal's patch needed for XML retrieval *******/
    xmlDoc *doc = NULL;
    xmlNode *root_element = NULL;
    /***** ending of Sukomal's patch for XML retrieval *******/

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "pp_xml");
        return (UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE(2, print_string, "Trace: entering pp_xml");

    ip->num_sections = 0;
    ip->mem_disp[0].begin_section = 0;
    ip->mem_disp[0].section_id = DISCARD_SECTION;

    current_secptr = &ip->pp_info->default_section;
    if (sm_buf->end <= 0) return(0);

    if (NULL == (doc = xmlReadMemory(sm_buf->buf, sm_buf->end, NULL, NULL,
                                     XML_PARSE_RECOVER|XML_PARSE_PEDANTIC))) {
        set_error(errno, "parsing XML document", "pp_xml");
        return UNDEF;
    }
    root_element = xmlDocGetRootElement(doc);
    assert(root_element->next == NULL);
    ip->curr_location = 0;
    ip->buf.end = 0;
    ip->num_nodes = 0;
    if (UNDEF == ip->create_sections(root_element, ip))
        return UNDEF;
    xmlFreeDoc(doc);
    if (UNDEF == pp_put_section ((PP_SECTIONS *) NULL, ip->curr_location, ip))
        return (UNDEF);

    /* traverse_tree has copied the content of "useful" tags into ip->buf.
     * setup output_doc->doc_text to point to this, so that subsequent 
     * routines (parser) will get the useful text in the expected place.
     * (see *8 of PROCEDURE DESCRIPTION above)
     */ 
    output_doc->doc_text = ip->buf.buf;
    sm_disp->sections = ip->mem_disp;
    sm_disp->num_sections = ip->num_sections;

    PRINT_TRACE(2, print_string, "Trace: leaving pp_xml");

    return (1);
}

int
close_pp_xml (inst)
int inst;
{
    STATIC_INFO *ip;

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_pp_xml");
        return (UNDEF);
    }
    ip = &info[inst];
    ip->valid_info = 0;

    PRINT_TRACE(2, print_string, "Trace: entering close_pp_xml");

    (void) free ((char *) ip->pp_info->pp_sections);
    (void) free ((char *) ip->pp_info);
    (void) free ((char *) ip->mem_disp);
    (void) free ((char *) ip->buf.buf);    
    (void) free (ip->stack);
    free(ip->nodelist);
    free(ip->supplist);

    xmlCleanupParser();

    PRINT_TRACE(2, print_string, "Trace: leaving close_pp_xml");

    return (0);
}


static int
create_sections_nonhier(a_node, ip)
xmlNode *a_node;
STATIC_INFO *ip;
{
    PP_SECTIONS *new_secptr;
    xmlNode *cur_node = NULL;

    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_TEXT_NODE) {
            if (lookup_section_start(cur_node->parent->name, 
                                     strlen(cur_node->parent->name),  
                                     &new_secptr, ip)) {
                if (UNDEF == add_buf_string(cur_node->content, &ip->buf) ||
                    UNDEF == pp_put_section(new_secptr, ip->curr_location, ip))
                    return (UNDEF);
                ip->curr_location += strlen(cur_node->content);
            }
        }
        create_sections_nonhier(cur_node->children, ip); 
    }
    return 1;
}

static int
create_sections_hier(root, ip)
xmlNode *root;
STATIC_INFO *ip;
{
    unsigned char *ptr;
    int num_nodes, top, status, suppIndex;
    PP_SECTIONS *new_secptr;
    xmlNode *cur_node = NULL;

    /* Count nodes and ensure adequate space on stack */
    num_nodes = count_nodes(root);
    if (num_nodes > ip->stack_size) {
        ip->stack_size += num_nodes;
        free(ip->stack);
        if (NULL == (ip->stack = Malloc(ip->stack_size, xmlNodePtr)))
            return(UNDEF);
    }

    /* Non-recursive DFS */
    /* 1. copies useful text into ip->buf.
     * 2. creates sections.
     * 3. sets up pointers from XML tree nodes into ip->buf.
     */
    ip->stack[0] = root;
    top = 0;
    while (top >= 0) {
        /* pop node from top of stack and process */
        cur_node = ip->stack[top];
        top--;
        cur_node->_private = (void *) -1; // initialisation of private-data

        if (cur_node->type == XML_TEXT_NODE &&
            lookup_section_start(cur_node->parent->name, 
                                 strlen(cur_node->parent->name),  
                                 &new_secptr, ip)) {
            if (UNDEF == (status = pp_copy_nw(cur_node->content, &ip->buf)))
                return UNDEF;
            if (status != 0) { // some non-whitespace chars found
                if (UNDEF == pp_put_section(new_secptr, ip->curr_location, ip))
                    return (UNDEF);
                ip->curr_location = ip->buf.end;

                if (ip->num_nodes == ip->max_nodes) {
                    ip->max_nodes *= 2;
                    if (NULL == (ip->nodelist = Realloc(ip->nodelist, 
                                                        ip->max_nodes,
                                                        xmlNodePtr)))
                        return(UNDEF);
                }
                ip->nodelist[ip->num_nodes] = cur_node;
                cur_node->_private = (void *) (ip->num_sections - 1);

                ip->num_nodes++;
            }
        }

        /* push children */
        for (cur_node = cur_node->last; cur_node; cur_node = cur_node->prev)
            ip->stack[++top] = cur_node;
    }

    ip->mem_disp[(int)ip->nodelist[ip->num_nodes-1]->_private].end_section = ip->curr_location;

/********************SUKOMAL START 17-08-2007 ******************************************
*
* We need to correctly set begin-pointer and end-pointer in ip->buf for each TEXT_NODE.
* 1. leaf-nodes: begin-pointer & end-pointer unchanged, 
* 2. intermediate nodes: 
*  	begin-pointer = that of its leftmost descendent 
*       end-pointer   =  that of its rightmost descendent.
*
*****************************************************************************************/
  

    if (UNDEF == (suppIndex = fix_sections(root, ip)))
    	return (UNDEF);

#if 0
    printf("Indexed text:\n");
    print_buf(&(ip->buf), NULL);

    printf("\n\n=======================================================\n\n"
           "Interesting nodes:\n\n");
	
    for (num_nodes = 0; num_nodes < suppIndex; num_nodes++) {
        ptr = xmlGetNodePath(ip->supplist[num_nodes].node);
        printf("SUPPLIST Path is:%s\n ", ptr);
        xmlFree(ptr);

        for (ptr = ip->buf.buf + ip->supplist[num_nodes].begin;
             ptr < ip->buf.buf + ip->supplist[num_nodes].end;
             ptr++)
            putchar(*ptr);

        printf("\n\n--------------------------------------------------\n\n");

    }

    for (num_nodes = 0; num_nodes < ip->num_sections; num_nodes++) {
        //ptr = (xmlChar *)ip->mem_disp[num_nodes].subsect;
        //printf("\n\nmem_disp Path is:%s\n\n", ptr);
        //xmlFree(ptr);

   	for (ptr = ip->buf.buf + ip->mem_disp[num_nodes].begin_section;
             ptr < ip->buf.buf + ip->mem_disp[num_nodes].end_section;
             ptr++)
     	   putchar(*ptr);

        printf("\n\n--------------------------------------------------\n\n");
	
    }

#endif
    //free(ptr);
    return 1;
}

/********************SUKOMAL START 17-08-2007 ******************************************/
static int
fix_sections(root, ip)
xmlNode *root;
STATIC_INFO *ip;
{
    unsigned char *ptr;
    int index, top, begin, end, num_node = 0;
    int flag_visited , *flag_stack;
    xmlNodePtr p, q, *local_stack;
    PP_SECTIONS *new_secptr;

    /* Non-recursive post-order traversal */

    if (NULL == (local_stack = Malloc(ip->stack_size, xmlNodePtr))||
        NULL == (flag_stack = Malloc(ip->stack_size, int)) ||
        NULL == (ip->supplist = Realloc(ip->supplist, ip->stack_size,SUPPLEMENTARY)))
        return UNDEF;

    local_stack[0] = root;
    flag_stack[0] = 0;

    top = 0;

    while (top >= 0) {
        /* pop node from top of stack and process */
        p = local_stack[top];
        flag_visited = flag_stack[top];
        top--;

      	if (flag_visited ==1) { /*PROCESS THE intermediate NODE*/            
	  if(lookup_section_start(p->name, strlen(p->name), &new_secptr, ip)){
		  for (index = 0; index < num_node; index++) {
                	if (ip->supplist[index].node->parent == p) {
                                begin = ip->supplist[index].begin;
                                break;
                        }
                  }

                  for (index = num_node-1; index >=0; index--) {
                        if (ip->supplist[index].node->parent == p) {
                                end = ip->supplist[index].end;
                                break;
                        }
                  }

                ip->supplist[num_node].node = p;
                ip->supplist[num_node].begin = begin;
                ip->supplist[num_node].end = end;
                num_node++;
		if (UNDEF == pp_put_section(new_secptr, begin, ip))
                    return (UNDEF);
                //ip->mem_disp[ip->num_sections - 1].subsect = (SM_DISP_SEC *)xmlGetNodePath(p);
		ip->mem_disp[ip->num_sections -1].end_section = end;
	  }//lookup_section

	} else if (p->children) {
                top++;
                flag_stack[top] = 1;
                local_stack[top] = p; /*push the node itself */
                /* and then push children */
                for (q = p->last; q; q = q->prev) {
                        top++;
                        flag_stack[top] = 0;
                        local_stack[top] = q;
                }
        } else if ((int)p->_private != -1) { /*LEAF NODE: POPULATE p->begin & p->end ACC TO (p->_private) data */
                ip->supplist[num_node].node = p;
                ip->supplist[num_node].begin = ip->mem_disp[(int)p->_private].begin_section;
                ip->supplist[num_node].end = ip->mem_disp[(int)p->_private].end_section;
                num_node++;
      } 
     
    } //WHILE ends
    free(local_stack);
    free(flag_stack);

    return (num_node);
}

/********************SUKOMAL END 17-08-2007 *******************************************/	
static int
count_nodes(root)
xmlNodePtr root;
{
    int num = 0;
    xmlNodePtr cur_node = NULL;

    for (cur_node = root->children; cur_node; cur_node = cur_node->next)
        num += count_nodes(cur_node);

    return num+1;
}


/* Append src string to sm_buf with whitespace normalization
 * (return 0 if src consisted only of whitespace, return 1 otherwise).
 */
static int
pp_copy_nw(src, sm_buf)
unsigned char *src;
SM_BUF *sm_buf;
{
    char in_ws; /* flag to keep track of whether currently in whitespace
                 * NB: safe to assume in_ws = 1 at start of section */
    unsigned char *p1, *p2;
    int i = strlen(src) + 1;

    if (sm_buf->end + i >= sm_buf->size) {
        sm_buf->size = sm_buf->size * 2 + i;
        if (NULL == (sm_buf->buf = Realloc(sm_buf->buf, sm_buf->size, 
                                           unsigned char)))
            return UNDEF;
    }

    for (p1 = src, p2 = sm_buf->buf + sm_buf->end, in_ws = 1; 
         *p1 != '\0'; 
         p1++) { 
        if (isspace(*p1)) {
            if (!in_ws) {
                *p2++ = ' ';
                in_ws = 1;
            }
        }
        else {
            *p2++ = *p1;
            in_ws = 0;
        }       
    }

    if (p2 == sm_buf->buf + sm_buf->end) // string consisted only of ws
        return 0;

    if (!in_ws) *p2++ = ' '; // ensure section ends in blank
    sm_buf->end = p2 - sm_buf->buf;

    return 1;
}


/***************************************************************************/
static int
pp_put_section (section, curr_loc, ip)
PP_SECTIONS *section;
long curr_loc;
STATIC_INFO *ip;
{
    /* End previous section */
    if (ip->num_sections > 0 &&
        ip->mem_disp[ip->num_sections - 1].end_section == 0)
        ip->mem_disp[ip->num_sections - 1].end_section = curr_loc;

    if (section == (PP_SECTIONS *) NULL || section->id == DISCARD_SECTION) {
        return (0);
    }

    if (ip->num_sections >= ip->max_num_sections) {
        if (NULL == (ip->mem_disp = (SM_DISP_SEC *)
                     realloc ((char *) ip->mem_disp,
                              (ip->max_num_sections + DISP_SEC_SIZE) *
                              sizeof (SM_DISP_SEC)))) {
            set_error (errno, "DISP_SEC malloc", "preparse");
            return (UNDEF);
        }
        ip->max_num_sections += DISP_SEC_SIZE;
    }
    ip->mem_disp[ip->num_sections].begin_section = curr_loc;
    ip->mem_disp[ip->num_sections].end_section = 0;
    ip->mem_disp[ip->num_sections].section_id = section->id;
    ip->num_sections++;
    return (0);
}


/* ******************** COPIED FROM pp_gen_line.c ******************** */

static int
init_pp_sections (spec, pp_info, qd_prefix)
SPEC *spec;
PP_INFO *pp_info;
char *qd_prefix;
{
    long i;
    char prefix_string[PATH_LEN];

    sect_prefix = prefix_string;
    pp_info->num_pp_sections = num_pp_sections + 1;

    if ((PP_SECTIONS *)0 == (pp_info->pp_sections = (PP_SECTIONS *)
                 malloc ((unsigned) (num_pp_sections+1)*sizeof (PP_SECTIONS))))
        return (UNDEF);
    for (i = 0; i < num_pp_sections; i++) {
	if (qd_prefix)
	    (void) sprintf (prefix_string, "%spp_section.%ld.",
			    qd_prefix, i);
	else (void) sprintf (prefix_string, "pp_section.%ld.", i);
        if (UNDEF == lookup_spec_prefix (spec,
                                         &sect_spec_args[0],
                                         num_sect_spec_args))
            return (UNDEF);
#ifdef TRACE
	if (global_trace && trace >= 4) {
            (void) fprintf (global_trace_fd, "%ld\t %s\t%c\t%s\t%d%d\n",
			    i, string, section_name[0], action,
			    oneline_flag, newdoc_flag);
	}
#endif // TRACE

        pp_info->pp_sections[i].name = string;
        pp_info->pp_sections[i].name_len = strlen (string);
        pp_info->pp_sections[i].id = section_name[0];
        if (0 == strcmp (action, "copy"))
            pp_info->pp_sections[i].parse = pp_copy;
        else if (0 == strcmp (action, "copy_nt"))
            pp_info->pp_sections[i].parse = pp_copy_nt;
        else if (0 == strcmp (action, "discard"))
            pp_info->pp_sections[i].parse = pp_discard;
        else if (0 == strcmp (action, "copy_nb"))
            pp_info->pp_sections[i].parse = pp_copy_nb;
        else {
            set_error (SM_ILLPA_ERR, "pp_section.action", "pp_named_text");
            return (UNDEF);
        }
        pp_info->pp_sections[i].flags = 0;
        if (oneline_flag)
            pp_info->pp_sections[i].flags |= PP_ONELINE;
        if (newdoc_flag)
            pp_info->pp_sections[i].flags |= PP_NEWDOC;
    }

    /* Add sentinel onto end */
    pp_info->pp_sections[num_pp_sections].name = "";
    pp_info->pp_sections[num_pp_sections].name_len = 0;
    pp_info->pp_sections[num_pp_sections].id = DISCARD_SECTION;
    pp_info->pp_sections[num_pp_sections].flags = 0;
    pp_info->pp_sections[num_pp_sections].parse = pp_discard;

    pp_info->default_section.name = "";
    pp_info->default_section.name_len = 0;
    pp_info->default_section.id = default_section_name[0];
    pp_info->default_section.flags = 0;
    if (0 == strcmp (default_section_action, "copy"))
            pp_info->default_section.parse = pp_copy;
        else if (0 == strcmp (default_section_action, "copy_nt"))
            pp_info->default_section.parse = pp_copy_nt;
        else if (0 == strcmp (default_section_action, "discard"))
            pp_info->default_section.parse = pp_discard;
        else if (0 == strcmp (default_section_action, "copy_nb"))
            pp_info->default_section.parse = pp_copy_nb;
        else {
            set_error (SM_ILLPA_ERR, "default_pp_action", "init_pp_named_text");
            return (UNDEF);
        }
    return (1);
}

/* Initialize lookup_table and do some simple sanity checking on the
   preparser sections array */
static int
init_lookup_section_start (sections, num_sections, ip)
PP_SECTIONS *sections;
long num_sections;
STATIC_INFO *ip;
{
    int i;
    char c;
    if (num_sections >= 256) {
        set_error (SM_INCON_ERR,
                   "Too many pp_sections",
                   "pp_preparse");
        return (UNDEF);
    }
    /* Sort pp_sections by decreasing alphabetic order */
    qsort ((char *) sections,
           (int) num_sections,
           sizeof (PP_SECTIONS),
           comp_pp_section);

    for (i = 0; i < 256; i++) {
        ip->lookup_table[i] = num_sections;
	ip->count_lookup_table[i] = 0;
    }
    for (i = 0; i < num_sections; i++) {
        c = sections[i].name[0];
	ip->count_lookup_table[(int) c]++;
        if (ip->lookup_table[(int) c] == num_sections) {
            ip->lookup_table[(int) c] = i;
        }
    }
    return (0);
}

/* Determine if this line starts a new section by comparing it
   with the section starters contained in pp_section
   Note section is sorted in descending alphabetic order.
   Return 1 if a new section is found, and set new_secptr.
   Return 0 if no new section is found
*/
static int
lookup_section_start (text, text_size, new_secptr, ip)
char *text;
long text_size;
PP_SECTIONS **new_secptr;
STATIC_INFO *ip;
{
    int i;
    PP_SECTIONS *sec_ptr, *end_sec_ptr;

    if (ip->pp_info->num_pp_sections > (i = ip->lookup_table[(unsigned char)*text])) {
        sec_ptr = &ip->pp_info->pp_sections[i];
        end_sec_ptr = sec_ptr + ip->count_lookup_table[(unsigned char)*text];
	while (sec_ptr < end_sec_ptr) {
            if (text_size >= sec_ptr->name_len &&
		0 == strncmp (text, 
			      sec_ptr->name, 
			      (int) sec_ptr->name_len)) {
		break;
	    }
            sec_ptr++;
        }
	if (sec_ptr < end_sec_ptr) {
            /* Have found new section starter. */
            *new_secptr = sec_ptr;
            return (1);
        }
    }
    return (0);
}

static int
comp_pp_section (ptr1, ptr2)
PP_SECTIONS *ptr1;
PP_SECTIONS *ptr2;
{
    return (strcmp (ptr2->name, ptr1->name));
}
