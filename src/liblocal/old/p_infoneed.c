#include "common.h"
#include "param.h"
#include "smart_error.h"
#include "functions.h"
#include "buf.h"
#include "docindex.h"
#include "vector.h"
#include "infoneed_parse.h"

#define PRINT_BITFIELD(mask, string) \
    if ((ifn_node->flags & mask) != (defaults.flags & mask)) { \
	(void) sprintf(buf, string, (ifn_node->flags & mask) != 0); \
	if (UNDEF == add_buf_string(buf, smbuf)) \
	    return(UNDEF); \
    }

static int
show_ifn_opts(ifn_node, smbuf)
IFN_TREE_NODE *ifn_node;
SM_BUF *smbuf;
{
    /* static */ IFN_TREE_NODE defaults;
    char buf[PATH_LEN], fmtbuf[PATH_LEN];		/* XXX */

    /* if (0 == defaults.op) -- make extra calls to save permanent space */
	init_ifn(&defaults);
    
    if (ifn_node->fuzziness != defaults.fuzziness) {
	(void) sprintf(buf, " fuzziness=%f", ifn_node->fuzziness);
	if (UNDEF == add_buf_string(buf, smbuf))
	    return(UNDEF);
    }

    if (ifn_node->weight != defaults.weight) {
	(void) sprintf(buf, " weight=%f", ifn_node->weight);
	if (UNDEF == add_buf_string(buf, smbuf))
	    return(UNDEF);
    }

    PRINT_BITFIELD(ADV_FLAGS_OUTPUT_QUERY, " output_query=%d");
    PRINT_BITFIELD(ADV_FLAGS_GET_DOCS, " get_docs=%d");
    PRINT_BITFIELD(ADV_FLAGS_REFORM, " reform=%d");
    PRINT_BITFIELD(ADV_FLAGS_EXPAND, " expand=%d");
    PRINT_BITFIELD(ADV_FLAGS_STEM, " stem=%d");
    PRINT_BITFIELD(ADV_FLAGS_HELP, " help=%d");
    PRINT_BITFIELD(ADV_FLAGS_EXPLAN, " explan=%d");
    PRINT_BITFIELD(ADV_FLAGS_MATCHES, " matches=%d");
    PRINT_BITFIELD(ADV_FLAGS_DOCFREQ, " docfreq=%d");
    PRINT_BITFIELD(ADV_FLAGS_RELEVANT, " relevant=%d");
    PRINT_BITFIELD(ADV_FLAGS_ORDERED, " ordered=%d");

    if (ifn_node->num_ret_docs != defaults.num_ret_docs) {
	(void) sprintf(buf, " num_ret_docs=%ld", ifn_node->num_ret_docs);
	if (UNDEF == add_buf_string(buf, smbuf))
	    return(UNDEF);
    }

    if (ifn_node->hr_type != defaults.hr_type) {
	(void) sprintf(buf, " hr_type=%ld", ifn_node->hr_type);
	if (UNDEF == add_buf_string(buf, smbuf))
	    return(UNDEF);
    }

    if (ifn_node->distance != defaults.distance) {
	(void) sprintf(buf, " distance=%ld", ifn_node->distance);
	if (UNDEF == add_buf_string(buf, smbuf))
	    return(UNDEF);
    }

    if (ifn_node->min_thresh != defaults.min_thresh) {
	(void) sprintf(buf, " min_thresh=%f", ifn_node->min_thresh);
	if (UNDEF == add_buf_string(buf, smbuf))
	    return(UNDEF);
    }

    if (ifn_node->max_thresh != defaults.max_thresh) {
	(void) sprintf(buf, " max_thresh=%f", ifn_node->max_thresh);
	if (UNDEF == add_buf_string(buf, smbuf))
	    return(UNDEF);
    }

    if (ifn_node->mergetype.buf != (char *)0) {
	(void) sprintf(fmtbuf, " mergetype=%%.%ds", ifn_node->mergetype.end);
	(void) sprintf(buf, fmtbuf, ifn_node->mergetype.buf);
	if (UNDEF == add_buf_string(buf, smbuf))
	    return(UNDEF);
    }

    if (ifn_node->span_start != defaults.span_start) {
	(void) sprintf(buf, " span_start=%ld", ifn_node->span_start);
	if (UNDEF == add_buf_string(buf, smbuf))
	    return(UNDEF);
    }

    if (ifn_node->span_end != defaults.span_end) {
	(void) sprintf(buf, " span_end=%ld", ifn_node->span_end);
	if (UNDEF == add_buf_string(buf, smbuf))
	    return(UNDEF);
    }

    return(1);
}

int
print_ifn_text(ifn_node, query_text, indent, outbuf)
IFN_TREE_NODE *ifn_node;
char *query_text;
int indent;
SM_BUF *outbuf;
{
    IFN_TREE_NODE *ifn_child;
    char fmtbuf[80], linebuf[PATH_LEN];
    static SM_BUF optbuf;
    int i;

    i = ifn_node->text_end - ifn_node->text_start + 1;
    if (PATH_LEN - 50 > i)
	(void) sprintf(fmtbuf, "%%-%dsop %%d covers '%%.%ds'\n", indent, i);
    else
	(void) sprintf(fmtbuf, "%%-%dsop %%d covers '%%.%ds...'\n", indent,
				PATH_LEN - 50);
    (void) sprintf(linebuf, fmtbuf, "", ifn_node->op,
					query_text + ifn_node->text_start);
    print_string(linebuf, outbuf);

    optbuf.end = 0;
    if (UNDEF == show_ifn_opts(ifn_node, &optbuf))
	return(UNDEF);
    if (optbuf.end > 0) {
	(void) sprintf(fmtbuf, "%%%d.%ds\n", optbuf.end + indent, optbuf.end);
	(void) sprintf(linebuf, fmtbuf, optbuf.buf);
	print_string(linebuf, outbuf);
    }

    if (ifn_node->convec.num_conwt > 0) {
	(void) sprintf(fmtbuf, "%%-%dsconcepts:", indent);
	(void) sprintf(linebuf, fmtbuf, "");
	print_string(linebuf, outbuf);
	for (i = 0; i <ifn_node->convec.num_conwt; i++) {
	    (void) sprintf(linebuf, " <%ld, %f>",
				ifn_node->convec.con_wtp[i].con,
				ifn_node->convec.con_wtp[i].wt);
	    print_string(linebuf, outbuf);
	}
	print_string("\n", outbuf);
    }

    for (ifn_child = ifn_node->child; ifn_child;
					ifn_child = ifn_child->sibling) {
	print_ifn_text(ifn_child, query_text, indent+4, outbuf);
    }

    if (0 == indent) {
	if (optbuf.size > 0) {
		(void) free(optbuf.buf);
		optbuf.size = 0;
	}
    }

    return(1);
}

void
debug_print_infoneed(ifn, outbuf)
IFN_PRINT *ifn;
SM_BUF *outbuf;
{
    (void) print_ifn_text(ifn->ifn_node, ifn->query_text, 0, outbuf);
}

void
inter_print_infoneed(ifn_node, query_text)
IFN_TREE_NODE *ifn_node;
char *query_text;
{
    (void) print_ifn_text(ifn_node, query_text, 0, (SM_BUF *)0);
}
