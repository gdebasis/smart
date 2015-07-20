#include	"pnorm_common.h"
#include	"common.h"

extern double robust_pow();


int
fuzzy_op_t1(op, xval, yval, param, result)
int	op;
double	xval;
double	yval;
float	param;
double	*result;
{
    if (op == AND_OP)
	*result = MIN(xval, yval);
    else
	*result = MAX(xval, yval); 
    return(1);
} /* fuzzy_op_t1 */


int
fuzzy_op_t2(op, xval, yval, param, result)
int	op;
double	xval;
double	yval;
float	param;
double	*result;
{
    if (op == AND_OP)
	*result = xval * yval;
    else
	*result = xval + yval - xval * yval; 
    return(1);
} /* fuzzy_op_t2 */


int
fuzzy_op_t3(op, xval, yval, param, result)
int	op;
double	xval;
double	yval;
float	param;
double	*result;
{
    if (op == AND_OP)
	*result = MAX(xval+yval-1.0, 0.0);
    else
	*result = MIN(xval+yval, 1.0); 
    return(1);
} /* fuzzy_op_t3 */


int
fuzzy_op_t4(op, xval, yval, param, result)
int	op;
double	xval;
double	yval;
float	param;
double	*result;
{
    if (op == AND_OP)
	*result = (xval*yval)/(xval+yval-xval*yval);
    else
	*result = (xval+yval-2.0*xval*yval)/(1.0-xval*yval);
    return(1);
} /* fuzzy_op_t4 */


int
fuzzy_op_t5(op, xval, yval, param, result)
int	op;
double	xval;
double	yval;
float	param;
double	*result;
{
    if (op == AND_OP) {
	if (xval == 1.0) *result = yval;
	else if (yval == 1.0) *result = xval;
	else *result = 0.0;
    }
    else {
	if (xval == 0.0) *result = yval;
	else if (yval == 0.0) *result = xval;
	else *result = 1.0;
    }
    return(1);
} /* fuzzy_op_t5 */


int
fuzzy_op_t6(op, xval, yval, param, result)
int	op;
double	xval;
double	yval;
float	param;
double	*result;
{
    if (op == AND_OP)
	*result = (param*xval*yval) /
		(1 - (1-param)*(xval+yval-xval*yval));
    else 
	*result = (param*(xval+yval)+xval*yval*(1-2*param)) /
		(param+xval*yval*(1-param));
    return(1);
} /* fuzzy_op_t6 */


int
fuzzy_op_t7(op, xval, yval, param, result)
int	op;
double	xval;
double	yval;
float	param;
double	*result;
{
    double	tmp;

    if (op == AND_OP) {
	tmp = 1.0 - robust_pow(robust_pow(1.0-xval,param)+
				   robust_pow(1.0-yval,param), 
			       1.0/param);
	*result = MAX(tmp, 0.0);
    }
    else {
	tmp = robust_pow(robust_pow(xval,param)+
				   robust_pow(yval,param),
			 1.0/param);
	*result = MIN(tmp, 1.0);
    }
    return(1);
} /* fuzzy_op_t7 */


int
fuzzy_op_t8(op, xval, yval, param, result)
int	op;
double	xval;
double	yval;
float	param;
double	*result;
{
    double	tmp;

    if (op == AND_OP) {
	tmp = robust_pow(robust_pow(1.0/xval-1.0,param)+
				robust_pow(1.0/yval-1.0,param),
			 1.0/param);
    }
    else {
	tmp = robust_pow(robust_pow(1.0/xval-1.0,-param)+
				robust_pow(1.0/yval-1.0,-param),
			 -1.0/param);
    }
    *result = 1.0 / (1.0+tmp);
    return(1);
} /* fuzzy_op_t8 */


int
fuzzy_op_t9(op, xval, yval, param, result)
int	op;
double	xval;
double	yval;
float	param;
double	*result;
{
    double	tmp;

    if (op == AND_OP) {
	tmp = MAX(xval,yval);
	*result = (xval*yval)/
			(MAX(tmp,param));
    }
    else {
	tmp = MAX(1.0-xval, 1.0-yval);
	*result = 1.0 - 
		(1.0-xval)*(1.0-yval)/ 
			(MAX(tmp,param));
    }
    return(1);
} /* fuzzy_op_t9 */


int
fuzzy_op_t10(op, xval, yval, param, result)
int	op;
double	xval;
double	yval;
float	param;
double	*result;
{
    if (op == AND_OP) {
	*result = MAX((xval+yval-1.0+param*xval*yval)/(1.0+param),
		      0.0);
    }
    else {
	*result = MIN(xval+yval+param*xval*yval,
		      1.0);
    }
    return(1);
} /* fuzzy_op_t10 */


int
fuzzy_op_t11(op, xval, yval, param, result)
int	op;
double	xval;
double	yval;
float	param;
double	*result;
{
    if (op == AND_OP) {
	*result = MAX((1.0+param)*(xval+yval-1.0)-param*xval*yval,
		      0.0);
    }
    else {
	*result = MIN(xval+yval+param*xval*yval,
		      1.0);
    }
    return(1);
} /* fuzzy_op_t11 */


