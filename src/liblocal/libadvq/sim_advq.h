typedef struct {
    Q_VEC_PAIR *vec_pair;
    long pos;			/* index of node/subtree to process */
    int ifn_inst;		/* inst for main sim_advq_vec stuff, as
				 * opposed to various op procedures, which we
				 * have to squirrel away to pass through the
				 * op procedures */
} IFN_SIM_VEC;
