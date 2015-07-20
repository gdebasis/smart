#ifndef ADVQ_OPH
#define ADVQ_OPH

typedef struct {
	char *op_name;
	PROC_INST parse_proc;
	PROC_INST retr_vector_proc;
	PROC_INST retr_invfile_proc;
} ADVQ_OP;

#endif	/* ADVQ_OPH */
