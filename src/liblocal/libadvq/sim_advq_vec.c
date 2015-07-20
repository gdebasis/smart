/* provide a retrieve.q_vec function to sim_seq */

/* sim_seq.c still says vec_vec in comments */

/*
QUERY_VECTOR *query_vec = RETRIEVAL->query
	long qid
	long node_num;
	char *vector

Q_VEC_PAIR vec_pair;
	QUERY_VECTOR *query_vec = RETRIEVAL->query
	VEC *dvec, filled by read_vector

Q_VEC_RESULT q_vec_result;
	float sim
	SM_BUF *explanation	-- allocate space if needed, always
					will start at beginning of buffer
*/

#include "common.h"
#include "param.h"
#include "spec.h"
#include "smart_error.h"
#include "functions.h"
#include "trace.h"
#include "inst.h"
#include "proc.h"
#include "advq_op.h"
#include "adv_vector.h"
#include "retrieve.h"
#include "sim_advq.h"

#define INIT_NUM_SIMS 100

static long num_ops;
static SPEC_PARAM spec_args[] = {
    /* should pick up "infoneed" from some other spec entry? */
    { "infoneed.num_ops",	getspec_long,	(char *) &num_ops },
    TRACE_PARAM ("advq_vec.trace")
};
static int num_spec_args = sizeof(spec_args) / sizeof(spec_args[0]);

static char *op_prefix;
static char *op_name;
static PROC_TAB *op_proc;
static SPEC_PARAM_PREFIX op_spec_args[] = {
    { &op_prefix, "name",	getspec_string,	(char *) &op_name },
    { &op_prefix, "retr_vec",	getspec_func,	(char *) &op_proc },
};
static int num_op_spec_args = sizeof(op_spec_args) / sizeof(op_spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    IFN_SIM_VEC ifn_sim_vec;
    SPEC *spec;			/* initial specification */
    ADVQ_OP *op_table;		/* table of advq operators */
    float *sims;		/* node-by-node similarity */
    int num_sims;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static int init_sim_advq();
static int close_sim_advq();
static int advvec_vec();
static float get_docwt();
static int in_eid_list();

static int
init_sim_advq(spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    STATIC_INFO *ip;
    int new_inst;
    long i;
    char prefix_string[PATH_LEN];
    ADVQ_OP *advq_op;

    NEW_INST(new_inst);
    if (UNDEF == new_inst)
	return(UNDEF);
    
    ip = &info[new_inst];
    ip->ifn_sim_vec.ifn_inst = new_inst;
    ip->spec = spec;

    if (UNDEF == lookup_spec(spec, &spec_args[0], num_spec_args))
	return(UNDEF);
    
    PRINT_TRACE(2, print_string, "Trace: entering init_sim_advq");

    ip->op_table = (ADVQ_OP *)
			malloc ((unsigned) ((num_ops+1) * sizeof(ADVQ_OP)));
    if ((ADVQ_OP *)0 == ip->op_table)
	return(UNDEF);
    
    op_prefix = prefix_string;
    advq_op = ip->op_table;
    for (i = 0; i < num_ops; i++) {
	/* use param_prefix */
	(void) sprintf(prefix_string, "infoneed.op.%ld.", i);
	if (UNDEF == lookup_spec_prefix(spec, &op_spec_args[0],
							num_op_spec_args))
	    return(UNDEF);
	advq_op->op_name = op_name;
	advq_op->retr_vector_proc.ptab = op_proc;
	advq_op->retr_vector_proc.inst = UNDEF;
	/* do not init procs -- that will be done as needed */
	/* set unused procs to null ?? */
	advq_op++;
    }
    advq_op->op_name = (char *)0;		/* sentinel */

    ip->sims = (float *) malloc ((unsigned)(INIT_NUM_SIMS * sizeof(float)));
    if ((float *)0 == ip->sims)
	return(UNDEF);
    ip->num_sims = INIT_NUM_SIMS;

    PRINT_TRACE(2, print_string, "Trace: leaving init_sim_advq");

    ip->valid_info = 1;

    return(new_inst);
}

static int
close_sim_advq(inst)
int inst;
{
    STATIC_INFO *ip;
    ADVQ_OP *advq_op;

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "close_sim_advq");
	return(UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE(2, print_string, "Trace: entering close_sim_advq");

    advq_op = ip->op_table;
    while (advq_op->op_name) {
	if (advq_op->retr_vector_proc.inst != UNDEF)
	    if (UNDEF == advq_op->retr_vector_proc.ptab->close_proc(
				advq_op->retr_vector_proc.inst))
		return(UNDEF);
	advq_op++;
    }
    (void) free((char *)ip->op_table);

    (void) free((char *)ip->sims);

    ip->valid_info = 0;

    PRINT_TRACE(2, print_string, "Trace: leaving close_sim_advq");

    return(0);
}


int
init_sim_advq_vec(spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
    return (init_sim_advq(spec, param_prefix));
}

/* is eid in eid_list (containing num_eids)? */
static int
in_eid_list(eid, eid_list, num_eids)
EID *eid, *eid_list;
long num_eids;
{
    long i;

    for (i = 0; i < num_eids; i++) {
	if ((eid->id == eid_list[i].id) &&
		(eid->ext.type == eid_list[i].ext.type) &&
		(eid->ext.num == eid_list[i].ext.num))
	    return(1);
    }
    return(0);
}

/* this is not the expected entry point, but no reason to throw it away */
int
sim_advq_vec(vec_pair, vec_result, inst)
Q_VEC_PAIR *vec_pair;
Q_VEC_RESULT *vec_result;
int inst;
{
    STATIC_INFO *ip;
    ADV_VEC *adv_vec;
    INFO_NEED *infoneed;
    long i, head;

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "sim_advq_vec");
	return(UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE(2, print_string, "Trace: entering sim_advq_vec");

    adv_vec = (ADV_VEC *) vec_pair->query_vec->vector;

    if (ip->num_sims < adv_vec->num_nodes) {
	ip->sims = (float *) realloc ((char *)ip->sims,
		(unsigned)((adv_vec->num_nodes+INIT_NUM_SIMS) * sizeof(float)));
	if ((float *)0 == ip->sims)
	    return(UNDEF);
	ip->num_sims = adv_vec->num_nodes;
    }

    vec_result->sim = 0.0;
    for (i = 0; i < adv_vec->num_infoneeds; i++) {
	infoneed = &(adv_vec->info_needs[i]);

	/* assume term and docid restrictions apply independently */

	/* is there a docid restriction that this document does not meet? */
	if (infoneed->num_docs > 0) {
	    if (!in_eid_list(&(vec_pair->dvec->id_num),
				&(adv_vec->docid_list[infoneed->doc_ptr]),
				infoneed->num_docs)) {
		vec_result->sim = 0.0;	/* ??? */
		return(0);		/* ??? */
	    }

	}
	/* check infoneed's restrict set term here XXX */

	head = infoneed->head_node;
	ip->ifn_sim_vec.vec_pair = vec_pair;
	ip->ifn_sim_vec.pos = head;
	if (UNDEF == advvec_vec(&(ip->ifn_sim_vec), vec_result))
	    return(UNDEF);

	/* process mergeinfos when defined */
	vec_result->sim += ip->sims[head];	/* ??? */
    }

    PRINT_TRACE(2, print_string, "Trace: leaving sim_advq_vec");
    return(1);
}

int
close_sim_advq_vec(inst)
int inst;
{
    return(close_sim_advq(inst));
}


static int
advvec_vec(ifn_sim_vec, vec_result, inst)
IFN_SIM_VEC *ifn_sim_vec;
Q_VEC_RESULT *vec_result;
int inst;
{
    STATIC_INFO *ip;
    ADV_VEC *query_vec;
    TREE_NODE *tree_ptr;
    long pos, curr_index;
    LEAF_NODE *current_leaf;
    OP_NODE *current_op;
    PROC_INST *op_proc;

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "advvec_vec");
	return(UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE(4, print_string, "Trace: entering advvec_vec");

    pos = ifn_sim_vec->pos;
    query_vec = (ADV_VEC *) ifn_sim_vec->vec_pair->query_vec->vector;
    tree_ptr = query_vec->tree + pos;
    curr_index = tree_ptr->info;

    /* is this a leaf? */
    if (UNDEF == tree_ptr->child) {
	current_leaf = query_vec->leaf_node_list + curr_index;
	ip->sims[pos] = get_docwt(ifn_sim_vec->vec_pair->dvec,
				current_leaf->ctype, current_leaf->concept);
	ip->sims[pos] *= tree_ptr->weight;	/* ??? */
	return(1);
    }

    /* it's an operator */
    current_op = query_vec->op_node_list + curr_index;

    /* initialize operator procedures if required */
    op_proc = &(ip->op_table[current_op->op].retr_vector_proc);
    if (UNDEF == op_proc->inst) {
	op_proc->inst = op_proc->ptab->init_proc(ip->spec, (char *)0);
	if (UNDEF == op_proc->inst)
	    return(UNDEF);
    }

    /* hand off to operator procedure */

    if (UNDEF == advvec_vec(ifn_sim_vec, vec_result, inst))
	return(UNDEF);

    PRINT_TRACE(4, print_string, "Trace: leaving advvec_vec");
    return(1);
}

static float
get_docwt(dvec, ctype, con)
VEC *dvec;
long ctype, con;
{
    long i, conbase;

    if (ctype >= dvec->num_ctype)
	return(0.0);
    
    conbase = 0;
    for (i = 0; i < ctype; i++)
	conbase += dvec->ctype_len[i];

    for (i = 0; i < dvec->ctype_len[ctype]; i++) {
	if (con == dvec->con_wtp[conbase+i].con)
	    return(dvec->con_wtp[conbase+i].wt);
    }
    
    return(0.0);
}
