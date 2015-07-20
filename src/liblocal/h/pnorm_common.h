#ifndef PNORM_COMMONH
#define PNORM_COMMONH
#define NOT_OP -2		       /* operators must be negative ints & */
#define AND_OP -3                      /*   must not be -1 (UNDEF)          */
#define OR_OP  -4

#define P_IP	   1.0		       /* p=1 is inner product */
#define P_INFINITY 0.0	               /* use 0 for p = infinity */

#define FUNDEF	-1.0

#define FPL     .29e-38         /* smallest floating pt. no. */
#define FPU     1.7e38          /* biggest floating pt. no. */
#define FPMAXL  70.0*log(10.0)  /* largest log of fl. pt. no. */

#define MALLOC_SIZE	1010    /* optimal size of block to allocate */

#endif /* PNORM_COMMONH */
