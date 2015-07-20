#ifndef BUFH
#define BUFH
/*        $Header: /home/smart/release/src/h/buf.h,v 11.2 1993/02/03 16:47:22 smart Exp $*/

/* structure used for passing around text (buf) which possibly includes
   NULLs.  see buf_util.c for add_buf(). */
typedef struct {
    int size;                      
    int end;
    unsigned char *buf;
} SM_BUF;

#endif /* BUFH */
