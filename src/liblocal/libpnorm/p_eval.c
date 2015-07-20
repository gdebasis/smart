#ifdef RCSID
static char rcsid[] = "$Header: /smart/package/src/lib_retrieve/p_eval.c,v 6.1 84/11/14 23:53:33 smart Exp $";
#endif

/* Copyright (c) 1984 - Gerard Salton, Chris Buckley, Ellen Voorhees */

#include "common.h"
#include "functions.h"
#include "param.h"
#include "pnorm_common.h"


double
eval_p_formula(p,type,num,denom)
register float p;	/* current p value */
int type;		/* type of operator */
double num;		/* numerator of formula: 
			   p=inf. & type=AND -> MAX(wt*1-x)
			   p=inf. & type=OR -> MAX(wt*x)
			   else sum of (wt*(1-x))**p (AND)
			            or (wt*x)**p (OR)	*/
double denom;		/* denominator of formula:
			   p=inf. -> MAX(q_wts)
			   else sum of q_wt**p	*/
{
    double quotient;		/* num / denom */
    double robust_pow();	/* returns a**b */

    quotient = (denom > FPL) ? (num/denom) : 0.0;

    switch (type) {
    case (AND_OP):
	if (p == P_INFINITY)
	    return(1.0 - quotient);
	else
	    return(1.0 - robust_pow(quotient,(double)1.0/(double)p));

    case (OR_OP):
	if (p == P_INFINITY)
	    return(quotient);
	else
	    return(robust_pow(quotient,(double)1.0/(double)p));

    default:
	(void) fprintf(stderr,"eval_p_formula: illegal node type %d.\n",type);
	return((double)UNDEF);
    } /* switch */
} /* eval_p_formula */



double
robust_pow(x,e)      
double x;	/* compute x**e checking for */
double e;	/*   underflow and overflow  */
{
    double new_exp;	/* new exponent */
    double val;		/* vlaue to be returned (x**e) */
    double log();
    double pow();
    double sqrt();

    if (x < 0.0) {
        (void) fprintf(stderr,"robust_pow: attempt to raise %7.4f to power.\n", x);
        return((double)UNDEF);
    }

    /* compute val depending on values of x and w */
    if (x == 0.0 || x == 1.0 || e == 1.0 ) {
	return(x);
    }
    if (e == 2.0 || e == 3.0) {	/* common integer exponents */
	val = x * x;
	if (e == 3.0) val *= x;
        return(val);
    }
    if (e == .5) {		/* square root */
	return(sqrt(x));
    }
    if (e == 1.5) {		/* a common case */
        return(x * sqrt(x));
    }
    if (e == 0.0) {
	return(1.0);
    }
    if (e == -1.0) {		/* reciprocal */
	return(1.0 / x);
    }

    /* general case */
    new_exp = e * log(x);
    if (new_exp > FPMAXL) {
	return(FPU);
    }
    if (new_exp < -FPMAXL) {
	return(FPL);
    }
    return(pow(x,e));

} /* robust_pow */  
