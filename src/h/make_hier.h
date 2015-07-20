#ifndef MAKE_HIERH
#define MAKE_HIERH
#define INIT	     1024
#define LEAF_LEVEL   1.0e10	       /* must be > all sim values */

#define ALLOC_LONG(p,n) { \
  if (NULL == (p = (long *) malloc((unsigned)(n)*sizeof(long)))) { \
    fprintf (stderr,"ALLOC_LONG: No more space - Quit\n"); \
    exit (1); \
  } \
}

#define ALLOC_NODES(cur_size,loc) { \
  if (loc >= cur_size) { \
    old_size = cur_size; \
    cur_size = loc+loc; \
    if (NULL == (char *)(nodes = \
                 (HIER *)realloc((char *)nodes,cur_size*sizeof(HIER)))) { \
      fprintf(stderr,"ALLOC_NODES: no more room\n"); \
      exit(1); \
    } \
    for (n_ptr=nodes+old_size; n_ptr < nodes+cur_size; n_ptr++) \
      n_ptr->node_num = UNDEF; \
  } \
}
#endif /* MAKE_HIERH */
