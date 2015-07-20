#include "common.h"
#include "param.h"
#include "functions.h"
#include "proc.h"

extern int init_ifn_parse_dn2(), ifn_parse_dn2(), close_ifn_parse_dn2();
extern int init_ifn_parse_ineed(), ifn_parse_ineed(), close_ifn_parse_ineed();
extern int init_ifn_parse_qtext(), ifn_parse_qtext(), close_ifn_parse_qtext();
extern int init_ifn_parse_fterm(), ifn_parse_fterm(), close_ifn_parse_fterm();
extern int init_ifn_parse_indep(), ifn_parse_indep(), close_ifn_parse_indep();
extern int init_ifn_parse_and(), ifn_parse_and(), close_ifn_parse_and();
extern int init_ifn_parse_or(), ifn_parse_or(), close_ifn_parse_or();
extern int init_ifn_parse_andnot(), ifn_parse_andnot(),
						close_ifn_parse_andnot();
extern int init_ifn_parse_hdrel(), ifn_parse_hdrel(), close_ifn_parse_hdrel();
extern int init_ifn_parse_other(), ifn_parse_other(), close_ifn_parse_other();
extern int init_ifn_parse_othargs(), ifn_parse_othargs(),
						close_ifn_parse_othargs();
extern int init_ifn_parse_comment(), ifn_parse_comment(),
						close_ifn_parse_comment();
extern int init_ifn_parse_annattr(), ifn_parse_annattr(),
						close_ifn_parse_annattr();
extern int init_ifn_parse_anntype(), ifn_parse_anntype(),
						close_ifn_parse_anntype();
extern int init_ifn_parse_atname(), ifn_parse_atname(),
						close_ifn_parse_atname();
extern int init_ifn_parse_attype(), ifn_parse_attype(),
						close_ifn_parse_attype();
extern int init_ifn_parse_atval(), ifn_parse_atval(), close_ifn_parse_atval();
extern int init_ifn_parse_apseinfo(), ifn_parse_apseinfo(),
						close_ifn_parse_apseinfo();
extern int init_ifn_parse_seapinfo(), ifn_parse_seapinfo(),
						close_ifn_parse_seapinfo();
extern int init_ifn_parse_seaphelp(), ifn_parse_seaphelp(),
						close_ifn_parse_seaphelp();
extern int init_ifn_parse_seapexpl(), ifn_parse_seapexpl(),
						close_ifn_parse_seapexpl();
extern int init_ifn_parse_seapmats(), ifn_parse_seapmats(),
						close_ifn_parse_seapmats();
extern int init_ifn_parse_mergeinfo(), ifn_parse_mergeinfo(),
						close_ifn_parse_mergeinfo();
extern int init_ifn_parse_weight(), ifn_parse_weight(),
						close_ifn_parse_weight();
extern int init_ifn_parse_collect(), ifn_parse_collect(),
						close_ifn_parse_collect();
extern int init_ifn_parse_restrict(), ifn_parse_restrict(),
						close_ifn_parse_restrict();
extern int init_ifn_parse_docid(), ifn_parse_docid(), close_ifn_parse_docid();
extern int init_ifn_parse_fdbkinfo(), ifn_parse_fdbkinfo(),
						close_ifn_parse_fdbkinfo();
extern int init_ifn_parse_docidrel(), ifn_parse_docidrel(),
						close_ifn_parse_docidrel();
extern int init_ifn_parse_textrel(), ifn_parse_textrel(),
						close_ifn_parse_textrel();
extern int init_ifn_parse_context(), ifn_parse_context(),
						close_ifn_parse_context();

PROC_TAB proc_infoneed_parse[] = {
	{ "dn2", init_ifn_parse_dn2, ifn_parse_dn2, close_ifn_parse_dn2 },
	{ "info_need", init_ifn_parse_ineed, ifn_parse_ineed,
						close_ifn_parse_ineed },
	{ "quoted_text", init_ifn_parse_qtext, ifn_parse_qtext,
						close_ifn_parse_qtext },
	{ "full_term", init_ifn_parse_fterm, ifn_parse_fterm,
						close_ifn_parse_fterm },
	{ "independent", init_ifn_parse_indep, ifn_parse_indep,
						close_ifn_parse_indep },
	{ "and", init_ifn_parse_and, ifn_parse_and, close_ifn_parse_and },
	{ "or", init_ifn_parse_or, ifn_parse_or, close_ifn_parse_or },
	{ "and_not", init_ifn_parse_andnot, ifn_parse_andnot,
						close_ifn_parse_andnot },
	{ "head_relation", init_ifn_parse_hdrel, ifn_parse_hdrel,
						close_ifn_parse_hdrel },
	{ "other_oper", init_ifn_parse_other, ifn_parse_other,
						close_ifn_parse_other },
	{ "other_oper_args", init_ifn_parse_othargs, ifn_parse_othargs,
						close_ifn_parse_othargs },
	{ "comment", init_ifn_parse_comment, ifn_parse_comment,
						close_ifn_parse_comment },
	{ "ann_attr", init_ifn_parse_annattr, ifn_parse_annattr,
						close_ifn_parse_annattr },
	{ "ann_type", init_ifn_parse_anntype, ifn_parse_anntype,
						close_ifn_parse_anntype },
	{ "attr_name", init_ifn_parse_atname, ifn_parse_atname,
						close_ifn_parse_atname },
	{ "attr_type", init_ifn_parse_attype, ifn_parse_attype,
						close_ifn_parse_attype },
	{ "attr_value", init_ifn_parse_atval, ifn_parse_atval,
						close_ifn_parse_atval },
	{ "app_se_info", init_ifn_parse_apseinfo, ifn_parse_apseinfo,
						close_ifn_parse_apseinfo },
	{ "se_app_info", init_ifn_parse_seapinfo, ifn_parse_seapinfo,
						close_ifn_parse_seapinfo },
	{ "se_app_help", init_ifn_parse_seaphelp, ifn_parse_seaphelp,
						close_ifn_parse_seaphelp },
	{ "se_app_expl", init_ifn_parse_seapexpl, ifn_parse_seapexpl,
						close_ifn_parse_seapexpl },
	{ "se_app_matches", init_ifn_parse_seapmats, ifn_parse_seapmats,
						close_ifn_parse_seapmats },
	{ "merge_info", init_ifn_parse_mergeinfo, ifn_parse_mergeinfo,
						close_ifn_parse_mergeinfo },
	{ "weight", init_ifn_parse_weight, ifn_parse_weight,
						close_ifn_parse_weight },
	{ "doc_collection", init_ifn_parse_collect, ifn_parse_collect,
						close_ifn_parse_collect },
	{ "restrict_set", init_ifn_parse_restrict, ifn_parse_restrict,
						close_ifn_parse_restrict },
	{ "docid", init_ifn_parse_docid, ifn_parse_docid,
						close_ifn_parse_docid },
	{ "feedback_info", init_ifn_parse_fdbkinfo, ifn_parse_fdbkinfo,
						close_ifn_parse_fdbkinfo },
	{ "docid_rel", init_ifn_parse_docidrel, ifn_parse_docidrel,
						close_ifn_parse_docidrel },
	{ "text_rel", init_ifn_parse_textrel, ifn_parse_textrel,
						close_ifn_parse_textrel },
	{ "context", init_ifn_parse_context, ifn_parse_context,
						close_ifn_parse_context },
};
static int num_proc_infoneed_parse =
	sizeof(proc_infoneed_parse) / sizeof(proc_infoneed_parse[0]);


extern int init_ifn_vec_dn2(), ifn_vec_dn2(), close_ifn_vec_dn2();
extern int init_ifn_vec_ineed(), ifn_vec_ineed(), close_ifn_vec_ineed();
extern int init_ifn_vec_qtext(), ifn_vec_qtext(), close_ifn_vec_qtext();
extern int init_ifn_vec_fterm(), ifn_vec_fterm(), close_ifn_vec_fterm();
extern int init_ifn_vec_indep(), ifn_vec_indep(), close_ifn_vec_indep();
extern int init_ifn_vec_and(), ifn_vec_and(), close_ifn_vec_and();
extern int init_ifn_vec_or(), ifn_vec_or(), close_ifn_vec_or();
extern int init_ifn_vec_andnot(), ifn_vec_andnot(), close_ifn_vec_andnot();
extern int init_ifn_vec_hdrel(), ifn_vec_hdrel(), close_ifn_vec_hdrel();
extern int init_ifn_vec_other(), ifn_vec_other(), close_ifn_vec_other();
extern int init_ifn_vec_othargs(), ifn_vec_othargs(), close_ifn_vec_othargs();
extern int init_ifn_vec_comment(), ifn_vec_comment(), close_ifn_vec_comment();
extern int init_ifn_vec_annattr(), ifn_vec_annattr(), close_ifn_vec_annattr();
extern int init_ifn_vec_anntype(), ifn_vec_anntype(), close_ifn_vec_anntype();
extern int init_ifn_vec_atname(), ifn_vec_atname(), close_ifn_vec_atname();
extern int init_ifn_vec_attype(), ifn_vec_attype(), close_ifn_vec_attype();
extern int init_ifn_vec_atval(), ifn_vec_atval(), close_ifn_vec_atval();
extern int init_ifn_vec_apseinfo(), ifn_vec_apseinfo(),
						close_ifn_vec_apseinfo();
extern int init_ifn_vec_seapinfo(), ifn_vec_seapinfo(),
						close_ifn_vec_seapinfo();
extern int init_ifn_vec_seaphelp(), ifn_vec_seaphelp(),
						close_ifn_vec_seaphelp();
extern int init_ifn_vec_seapexpl(), ifn_vec_seapexpl(),
						close_ifn_vec_seapexpl();
extern int init_ifn_vec_seapmats(), ifn_vec_seapmats(),
						close_ifn_vec_seapmats();
extern int init_ifn_vec_mergeinfo(), ifn_vec_mergeinfo(),
						close_ifn_vec_mergeinfo();
extern int init_ifn_vec_weight(), ifn_vec_weight(), close_ifn_vec_weight();
extern int init_ifn_vec_collect(), ifn_vec_collect(), close_ifn_vec_collect();
extern int init_ifn_vec_restrict(), ifn_vec_restrict(),
						close_ifn_vec_restrict();
extern int init_ifn_vec_docid(), ifn_vec_docid(), close_ifn_vec_docid();
extern int init_ifn_vec_fdbkinfo(), ifn_vec_fdbkinfo(),
						close_ifn_vec_fdbkinfo();
extern int init_ifn_vec_docidrel(), ifn_vec_docidrel(),
						close_ifn_vec_docidrel();
extern int init_ifn_vec_textrel(), ifn_vec_textrel(), close_ifn_vec_textrel();
extern int init_ifn_vec_context(), ifn_vec_context(), close_ifn_vec_context();

PROC_TAB proc_infoneed_vector[] = {
	{ "dn2", init_ifn_parse_dn2, ifn_parse_dn2, close_ifn_parse_dn2 },
/*	{ "dn2", init_ifn_vec_dn2, ifn_vec_dn2, close_ifn_vec_dn2 },
	{ "info_need", init_ifn_vec_ineed, ifn_vec_ineed, close_ifn_vec_ineed },
	{ "quoted_text", init_ifn_vec_qtext, ifn_vec_qtext,
						close_ifn_vec_qtext },
	{ "full_term", init_ifn_vec_fterm, ifn_vec_fterm,
						close_ifn_vec_fterm },
	{ "independent", init_ifn_vec_indep, ifn_vec_indep,
						close_ifn_vec_indep },
	{ "and", init_ifn_vec_and, ifn_vec_and, close_ifn_vec_and },
	{ "or", init_ifn_vec_or, ifn_vec_or, close_ifn_vec_or },
	{ "and_not", init_ifn_vec_andnot, ifn_vec_andnot,
						close_ifn_vec_andnot },
	{ "head_relation", init_ifn_vec_hdrel, ifn_vec_hdrel,
						close_ifn_vec_hdrel },
	{ "other_oper", init_ifn_vec_other, ifn_vec_other,
						close_ifn_vec_other },
	{ "other_oper_args", init_ifn_vec_othargs, ifn_vec_othargs,
						close_ifn_vec_othargs },
	{ "comment", init_ifn_vec_comment, ifn_vec_comment,
						close_ifn_vec_comment },
	{ "ann_attr", init_ifn_vec_annattr, ifn_vec_annattr,
						close_ifn_vec_annattr },
	{ "ann_type", init_ifn_vec_anntype, ifn_vec_anntype,
						close_ifn_vec_anntype },
	{ "attr_name", init_ifn_vec_atname, ifn_vec_atname,
						close_ifn_vec_atname },
	{ "attr_type", init_ifn_vec_attype, ifn_vec_attype,
						close_ifn_vec_attype },
	{ "attr_value", init_ifn_vec_atval, ifn_vec_atval,
						close_ifn_vec_atval },
	{ "app_se_info", init_ifn_vec_apseinfo, ifn_vec_apseinfo,
						close_ifn_vec_apseinfo },
	{ "se_app_info", init_ifn_vec_seapinfo, ifn_vec_seapinfo,
						close_ifn_vec_seapinfo },
	{ "se_app_help", init_ifn_vec_seaphelp, ifn_vec_seaphelp,
						close_ifn_vec_seaphelp },
	{ "se_app_expl", init_ifn_vec_seapexpl, ifn_vec_seapexpl,
						close_ifn_vec_seapexpl },
	{ "se_app_matches", init_ifn_vec_seapmats, ifn_vec_seapmats,
						close_ifn_vec_seapmats },
	{ "merge_info", init_ifn_vec_mergeinfo, ifn_vec_mergeinfo,
						close_ifn_vec_mergeinfo },
	{ "weight", init_ifn_vec_weight, ifn_vec_weight, close_ifn_vec_weight },
	{ "doc_collection", init_ifn_vec_collect, ifn_vec_collect,
						close_ifn_vec_collect },
	{ "restrict_set", init_ifn_vec_restrict, ifn_vec_restrict,
						close_ifn_vec_restrict },
	{ "docid", init_ifn_vec_docid, ifn_vec_docid, close_ifn_vec_docid },
	{ "feedback_info", init_ifn_vec_fdbkinfo, ifn_vec_fdbkinfo,
						close_ifn_vec_fdbkinfo },
	{ "docid_rel", init_ifn_vec_docidrel, ifn_vec_docidrel,
						close_ifn_vec_docidrel },
	{ "text_rel", init_ifn_vec_textrel, ifn_vec_textrel,
						close_ifn_vec_textrel },
	{ "context", init_ifn_vec_context, ifn_vec_context,
						close_ifn_vec_context },
*/
};
static int num_proc_infoneed_vector =
	sizeof(proc_infoneed_vector) / sizeof(proc_infoneed_vector[0]);


extern int init_ifn_invf_dn2(), ifn_invf_dn2(), close_ifn_invf_dn2();
extern int init_ifn_invf_ineed(), ifn_invf_ineed(), close_ifn_invf_ineed();
extern int init_ifn_invf_qtext(), ifn_invf_qtext(), close_ifn_invf_qtext();
extern int init_ifn_invf_fterm(), ifn_invf_fterm(), close_ifn_invf_fterm();
extern int init_ifn_invf_indep(), ifn_invf_indep(), close_ifn_invf_indep();
extern int init_ifn_invf_and(), ifn_invf_and(), close_ifn_invf_and();
extern int init_ifn_invf_or(), ifn_invf_or(), close_ifn_invf_or();
extern int init_ifn_invf_andnot(), ifn_invf_andnot(), close_ifn_invf_andnot();
extern int init_ifn_invf_hdrel(), ifn_invf_hdrel(), close_ifn_invf_hdrel();
extern int init_ifn_invf_other(), ifn_invf_other(), close_ifn_invf_other();
extern int init_ifn_invf_othargs(), ifn_invf_othargs(),
						close_ifn_invf_othargs();
extern int init_ifn_invf_comment(), ifn_invf_comment(),
						close_ifn_invf_comment();
extern int init_ifn_invf_annattr(), ifn_invf_annattr(),
						close_ifn_invf_annattr();
extern int init_ifn_invf_anntype(), ifn_invf_anntype(),
						close_ifn_invf_anntype();
extern int init_ifn_invf_atname(), ifn_invf_atname(), close_ifn_invf_atname();
extern int init_ifn_invf_attype(), ifn_invf_attype(), close_ifn_invf_attype();
extern int init_ifn_invf_atval(), ifn_invf_atval(), close_ifn_invf_atval();
extern int init_ifn_invf_apseinfo(), ifn_invf_apseinfo(),
						close_ifn_invf_apseinfo();
extern int init_ifn_invf_seapinfo(), ifn_invf_seapinfo(),
						close_ifn_invf_seapinfo();
extern int init_ifn_invf_seaphelp(), ifn_invf_seaphelp(),
						close_ifn_invf_seaphelp();
extern int init_ifn_invf_seapexpl(), ifn_invf_seapexpl(),
						close_ifn_invf_seapexpl();
extern int init_ifn_invf_seapmats(), ifn_invf_seapmats(),
						close_ifn_invf_seapmats();
extern int init_ifn_invf_mergeinfo(), ifn_invf_mergeinfo(),
						close_ifn_invf_mergeinfo();
extern int init_ifn_invf_weight(), ifn_invf_weight(), close_ifn_invf_weight();
extern int init_ifn_invf_collect(), ifn_invf_collect(),
						close_ifn_invf_collect();
extern int init_ifn_invf_restrict(), ifn_invf_restrict(),
						close_ifn_invf_restrict();
extern int init_ifn_invf_docid(), ifn_invf_docid(), close_ifn_invf_docid();
extern int init_ifn_invf_fdbkinfo(), ifn_invf_fdbkinfo(),
						close_ifn_invf_fdbkinfo();
extern int init_ifn_invf_docidrel(), ifn_invf_docidrel(),
						close_ifn_invf_docidrel();
extern int init_ifn_invf_textrel(), ifn_invf_textrel(),
						close_ifn_invf_textrel();
extern int init_ifn_invf_context(), ifn_invf_context(),
						close_ifn_invf_context();

PROC_TAB proc_infoneed_invfile[] = {
	{ "dn2", init_ifn_parse_dn2, ifn_parse_dn2, close_ifn_parse_dn2 },
/*	{ "dn2", init_ifn_invf_dn2, ifn_invf_dn2, close_ifn_invf_dn2 },
	{ "info_need", init_ifn_invf_ineed, ifn_invf_ineed,
						close_ifn_invf_ineed },
	{ "quoted_text", init_ifn_invf_qtext, ifn_invf_qtext,
						close_ifn_invf_qtext },
	{ "full_term", init_ifn_invf_fterm, ifn_invf_fterm,
						close_ifn_invf_fterm },
	{ "independent", init_ifn_invf_indep, ifn_invf_indep,
						close_ifn_invf_indep },
	{ "and", init_ifn_invf_and, ifn_invf_and, close_ifn_invf_and },
	{ "or", init_ifn_invf_or, ifn_invf_or, close_ifn_invf_or },
	{ "and_not", init_ifn_invf_andnot, ifn_invf_andnot,
						close_ifn_invf_andnot },
	{ "head_relation", init_ifn_invf_hdrel, ifn_invf_hdrel,
						close_ifn_invf_hdrel },
	{ "other_oper", init_ifn_invf_other, ifn_invf_other,
						close_ifn_invf_other },
	{ "other_oper_args", init_ifn_invf_othargs, ifn_invf_othargs,
						close_ifn_invf_othargs },
	{ "comment", init_ifn_invf_comment, ifn_invf_comment,
						close_ifn_invf_comment },
	{ "ann_attr", init_ifn_invf_annattr, ifn_invf_annattr,
						close_ifn_invf_annattr },
	{ "ann_type", init_ifn_invf_anntype, ifn_invf_anntype,
						close_ifn_invf_anntype },
	{ "attr_name", init_ifn_invf_atname, ifn_invf_atname,
						close_ifn_invf_atname },
	{ "attr_type", init_ifn_invf_attype, ifn_invf_attype,
						close_ifn_invf_attype },
	{ "attr_value", init_ifn_invf_atval, ifn_invf_atval,
						close_ifn_invf_atval },
	{ "app_se_info", init_ifn_invf_apseinfo, ifn_invf_apseinfo,
						close_ifn_invf_apseinfo },
	{ "se_app_info", init_ifn_invf_seapinfo, ifn_invf_seapinfo,
						close_ifn_invf_seapinfo },
	{ "se_app_help", init_ifn_invf_seaphelp, ifn_invf_seaphelp,
						close_ifn_invf_seaphelp },
	{ "se_app_expl", init_ifn_invf_seapexpl, ifn_invf_seapexpl,
						close_ifn_invf_seapexpl },
	{ "se_app_matches", init_ifn_invf_seapmats, ifn_invf_seapmats,
						close_ifn_invf_seapmats },
	{ "merge_info", init_ifn_invf_mergeinfo, ifn_invf_mergeinfo,
						close_ifn_invf_mergeinfo },
	{ "weight", init_ifn_invf_weight, ifn_invf_weight,
						close_ifn_invf_weight },
	{ "doc_collection", init_ifn_invf_collect, ifn_invf_collect,
						close_ifn_invf_collect },
	{ "restrict_set", init_ifn_invf_restrict, ifn_invf_restrict,
						close_ifn_invf_restrict },
	{ "docid", init_ifn_invf_docid, ifn_invf_docid,
						close_ifn_invf_docid },
	{ "feedback_info", init_ifn_invf_fdbkinfo, ifn_invf_fdbkinfo,
						close_ifn_invf_fdbkinfo },
	{ "docid_rel", init_ifn_invf_docidrel, ifn_invf_docidrel,
						close_ifn_invf_docidrel },
	{ "text_rel", init_ifn_invf_textrel, ifn_invf_textrel,
						close_ifn_invf_textrel },
	{ "context", init_ifn_invf_context, ifn_invf_context,
						close_ifn_context },
*/
};
static int num_proc_infoneed_invfile =
	sizeof(proc_infoneed_invfile) / sizeof(proc_infoneed_invfile[0]);

TAB_PROC_TAB proc_infoneed_op[] = {
	{ "parse",	TPT_PROC, NULL, proc_infoneed_parse,
					&num_proc_infoneed_parse },
	{ "vector",	TPT_PROC, NULL, proc_infoneed_vector,
					&num_proc_infoneed_vector },
	{ "invfile",	TPT_PROC, NULL, proc_infoneed_invfile,
					&num_proc_infoneed_invfile },
};
static int num_proc_infoneed_op =
	sizeof(proc_infoneed_op) / sizeof(proc_infoneed_op[0]);



extern int init_infoneed_index(), infoneed_index(), close_infoneed_index();
PROC_TAB proc_infoneed_index[] = {
	{ "standard", init_infoneed_index, infoneed_index,
						close_infoneed_index },
};
static int num_proc_infoneed_index =
	sizeof(proc_infoneed_index) / sizeof(proc_infoneed_index[0]);

extern int init_infoneed_parse(), infoneed_parse(), close_infoneed_parse();
extern int init_infoneed_dummy_parse(), infoneed_dummy_parse(),
						close_infoneed_dummy_parse();
PROC_TAB proc_infoneed_ifn_tree[] = {
	{ "standard", init_infoneed_parse, infoneed_parse,
						close_infoneed_parse },
	{ "dummy", init_infoneed_dummy_parse, infoneed_dummy_parse,
						close_infoneed_dummy_parse },
};
static int num_proc_infoneed_ifn_tree =
	sizeof(proc_infoneed_ifn_tree) / sizeof(proc_infoneed_ifn_tree[0]);

TAB_PROC_TAB proc_infoneed_section[] = {
	{ "index",	TPT_PROC, NULL, proc_infoneed_index,
					&num_proc_infoneed_index },
	{ "ifn_tree",	TPT_PROC, NULL, proc_infoneed_ifn_tree,
					&num_proc_infoneed_ifn_tree },
};
static int num_proc_infoneed_section =
	sizeof(proc_infoneed_section) / sizeof(proc_infoneed_section[0]);



TAB_PROC_TAB lproc_infoneed[] = {
	{ "op",		TPT_TAB, proc_infoneed_op, NULL,
						&num_proc_infoneed_op },
	{ "section",	TPT_TAB, proc_infoneed_section, NULL,
						&num_proc_infoneed_section },
};
int num_lproc_infoneed =
	sizeof(lproc_infoneed) / sizeof(lproc_infoneed[0]);
