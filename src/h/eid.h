#ifndef EIDH
#define EIDH
/*        $Header: /home/smart/release/src/h/eid.h,v 11.2 1993/02/03 16:47:44 smart Exp $*/

typedef struct {
    long id;
    struct {
	unsigned int type:4;
	unsigned int num:28;
    } ext;
} EID;

typedef struct {
    long num_eids;
    EID *eid;
} EID_LIST;

#define P_WILD_EID ((1<<28) - 1)

#define EXT_NONE(x) (x.type = x.num = 0)

#endif /* EIDH */
