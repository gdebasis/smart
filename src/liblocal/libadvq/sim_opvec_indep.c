#include "sim_advq.h"

int
init_ifn_vec_indep(spec, param_prefix)
SPEC *spec;
char *param_prefix;
{
}

int
ifn_vec_indep(ifn_sim_vec, vec_result, inst)
IFN_SIM_VEC *ifn_sim_vec;
Q_VEC_RESULT *vec_result;
int inst;
{
    Q_VEC_PAIR *vec_pair = ifn_sim_vec->vec_pair;
    long pos = ifn_sim_vec->pos;

    if (UNDEF == advvec_vec(vec_pair, vec_result, ifn_sim_vec->ifn_inst, 0L))
	    return(UNDEF);
}

int
close_ifn_vec_indep(inst)
int inst;
{
}

