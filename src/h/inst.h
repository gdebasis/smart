#ifndef INSTH
#define INSTH
/*  $Header: /home/smart/release/src/h/inst.h,v 11.2 1993/02/03 16:47:32 smart Exp $ */

/* Macros for aiding in creating new storage for static variables for a new 
 * instantiation of a procedure.  Usual procedure skeleton will look like:
 *  * Static info to be kept for each instantiation of this proc *
 *  typedef struct {
 *      * bookkeeping *
 *      int valid_info;
 *      * procedure dependent info *
 *      ...
 *  } STATIC_INFO;
 *
 *  static STATIC_INFO *info;
 *  static int num_inst = 0;
 *  static int max_inst = 0;
 *  
 *  int
 *  init_proc (spec, param)
 *  SPEC *spec;
 *  char *param;
 *  {
 *      STATIC_INFO *ip;
 *      int new_inst;
 *      if (num_inst == 0) {
 *          if (UNDEF == lookup_spec (spec,
 *                                    &spec_args[0],
 *                                    num_spec_args))
 *              return (UNDEF);
 *      }
 *      NEW_INST (new_inst);
 *      if (UNDEF == new_inst)
 *          return (UNDEF);
 *      ip = &info[new_inst];
 *  ...
 *      return (new_inst);
 *  }
 *
 *  int
 *  proc (param1, param2, inst)
 *  char *param1, *param2;
 *  int inst;
 *  {
 *      STATIC_INFO *ip;
 *      if (! VALID_INST (inst)) {
 *          set_error (SM_ILLPA_ERR, "vec_inv", "Illegal instantiation value");
 *          return (UNDEF);
 *      }
 *      ip  = &info[inst];
 *      ...
*/


#define NEW_INST(i) \
     {for (i = 0; i < max_inst && info[i].valid_info; i++) \
         ; \
     if (i == max_inst) { \
         max_inst++; \
         if (i == 0) { \
             if (NULL == (info = (STATIC_INFO *) \
                          malloc (sizeof (STATIC_INFO)))) \
                 i = UNDEF; \
         } \
         else { \
             if (NULL == (info = (STATIC_INFO *) \
                          realloc ((char *) info, \
                                   sizeof (STATIC_INFO) \
                                   * (unsigned) max_inst))) \
                 i = UNDEF; \
         } \
         if (i != UNDEF) info[i].valid_info = 0; \
     }}


#define VALID_INST(i) \
    (i >= 0 && i < max_inst && info[i].valid_info)

#endif /* INSTH */
