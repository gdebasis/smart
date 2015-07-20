#ifndef COMMONH
#define COMMONH
/*        $Header: /home/smart/release/src/h/common.h,v 11.2 1993/02/03 16:47:23 smart Exp $*/

#ifndef FALSE
#define FALSE   0
#endif
#ifndef TRUE
#define TRUE    1
#endif

#define UNDEF   -1

#define EPSILON 1e-7
/*
 * For debugging.  To make the asserts all become no-ops, uncomment out
 * the definition of NDEBUG.
 */
/* #define NDEBUG */
#include <assert.h>

#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))
#define ISBETWEEN(x,a,b) (((a) < (x)) && ((x) <= (b)))
#endif

/*
 * Some useful macros for making malloc et al easier to use.
 * Macros handle the casting and the like that's needed.
 */
#define Malloc(n,type) (type *) malloc( (unsigned) ((n)*sizeof(type)))
#define Realloc(loc,n,type) (type *) realloc( (char *)(loc), \
                                              (unsigned) ((n)*sizeof(type)))
#define Free(loc) (void) free( (char *)(loc) )
#define Bcopy(src,dest,len) bcopy( (char *)(src), (char *)(dest), (int)(len) )

/*
 * Define prototypes as:  function _AP((arg1,arg2));
 * For ANSI C, that will be expanded to a prototype.
 * Otherwise, it'll become a normal function declaration.
 * ("AP" stands for "argument prototypes"; seems to be used commonly).
 */
#ifdef __STDC__
#define _AP(args) args
#else
#define _AP(args) ()
#endif

#endif /* COMMONH */

