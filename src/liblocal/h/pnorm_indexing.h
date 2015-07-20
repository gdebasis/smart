#ifndef PNORM_INDEXINGH
#define PNORM_INDEXINGH
typedef struct {
  short type;                          /* operator type or UNDEF if concept */
  short ctype;			       /* concept type if con or UNDEF */
  int node;			       /* tree node number */
  long con;			       /* concept number or UNDEF */
  float p;			       /* p-value or FUNDEF */
  float wt;			       /* weight */
} NODE_INFO;

#define ANDOR -10
#endif /* PNORM_INDEXINGH */
