#ifndef CONTEXTH
#define CONTEXTH
/*  $Header: /home/smart/release/src/h/context.h,v 11.2 1993/02/03 16:47:23 smart Exp $ */

/* Global variable describing context in which procedures are executing.
   For example, if weighting a vector, need to know whether to use
   doc_weight or query_weight values. (eg. context & INDEXING_DOC)
*/
extern long global_context;

#define CTXT_DOC (1)
#define CTXT_QUERY (1 << 1)
#define CTXT_INDEXING (1 << 2)
#define CTXT_INDEXING_DOC (CTXT_DOC | CTXT_INDEXING)
#define CTXT_INDEXING_QUERY (CTXT_QUERY | CTXT_INDEXING)

/* CTXT_LOCAL is defined for those procedures that want to define their
   own contexts, but don't want the definitions here.  All contexts without
   CTXT_LOCAL set are guaranteed to be accurate (and should be defined above).
   All contexts with CTXT_LOCAL set have no such guarantee. 
*/
#define CTXT_LOCAL ((unsigned)1 << 31)
#define CTXT_SECT (CTXT_LOCAL | (1 << 16))
#define CTXT_PARA (CTXT_LOCAL | (1 << 17))
#define CTXT_SENT (CTXT_LOCAL | (1 << 18))

#define set_context(x) global_context = (x)
#define check_context(x) ((x) == (global_context & (x)))
#define get_context() global_context
#define clr_context() global_context = 0
#define add_context(x) global_context |= (x)
#define del_context(x) global_context &= (~(x))

typedef unsigned long CONTEXT;

#endif /* CONTEXTH */
