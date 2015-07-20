#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libgeneral/vec_util.c,v 11.2 1993/02/03 16:50:45 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "trace.h"
#include "functions.h"
#include "vector.h"

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Copy internal arrays of vec to ensure they are not overwritten
 *2 save_vec (vec)
 *3   VEC *vec;
 *4 free_vec (vec)
 *7 Save_vec saves a vector for future use (remember that the contents of
 *7 internal arrays of an object are not guaranteed to exist past the next
 *7 invocation of the procedure that produced them).
 *7 free_vec frees a vector that was previously saved.
 *8 mallocs space for ctype_len and con_wtp, and copies values into them
***********************************************************************/
int 
save_vec (vec)
VEC *vec;
{
    long *ctype_buf;
    CON_WT *conwt_buf;

    if (vec->num_conwt == 0)
        return (0);
    if (NULL == (ctype_buf = (long *)
                 malloc ((unsigned) vec->num_ctype * sizeof (long))) ||
        NULL == (conwt_buf = (CON_WT *)
                 malloc ((unsigned) vec->num_conwt * sizeof (CON_WT)))) 
        return (UNDEF);

    bcopy ((char *) vec->ctype_len,
           (char *) ctype_buf,
           (int) vec->num_ctype * sizeof (long));
    bcopy ((char *) vec->con_wtp,
           (char *) conwt_buf,
           (int) vec->num_conwt * sizeof (CON_WT));

    vec->ctype_len = ctype_buf;
    vec->con_wtp = conwt_buf;
    return (1);
}


/* Free vecs that have been previously saved with save_vec */
int
free_vec (vec)
VEC *vec;
{
    if (vec->num_conwt == 0)
        return (0);
    (void) free ((char *) vec->ctype_len);
    (void) free ((char *) vec->con_wtp);

    return (1);
}


/********************   PROCEDURE DESCRIPTION   ************************
 *0 Copy entire vector orig_vec to dest_vec
 *2 copy_vec (dest_vec, orig_vec)
 *3   VEC *dest_vec;
 *3   VEC *orig_vec;
 *8 mallocs space for ctype_len and con_wtp, and copies values into them
***********************************************************************/
int 
copy_vec (dest_vec, orig_vec)
VEC *dest_vec;
VEC *orig_vec;
{
    long *ctype_buf = (long *) NULL;
    CON_WT *conwt_buf = (CON_WT *) NULL;

    if (orig_vec->num_conwt != 0) {
        if (NULL == (ctype_buf = (long *)
                   malloc ((unsigned) orig_vec->num_ctype * sizeof (long))) ||
            NULL == (conwt_buf = (CON_WT *)
                   malloc ((unsigned) orig_vec->num_conwt * sizeof (CON_WT)))) 
            return (UNDEF);
        
        bcopy ((char *) orig_vec->ctype_len,
               (char *) ctype_buf,
               (int) orig_vec->num_ctype * sizeof (long));
        bcopy ((char *) orig_vec->con_wtp,
               (char *) conwt_buf,
               (int) orig_vec->num_conwt * sizeof (CON_WT));
    }

    dest_vec->ctype_len = ctype_buf;
    dest_vec->con_wtp = conwt_buf;
    dest_vec->id_num = orig_vec->id_num;
    dest_vec->num_ctype = orig_vec->num_ctype;
    dest_vec->num_conwt = orig_vec->num_conwt;

    return (1);
}

int merge_vec_pair(VEC_PAIR* vec_pair, VEC* ovec) {
	return merge_vec(vec_pair->vec1, vec_pair->vec2, ovec);
}

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Merge vec1 and vec2 into an output vector which is allocated
 *  by this procedure itself
 *2 merge_vec (vec1, vec2, ovec)
 *3   VEC *vec1;
 *3   VEC *vec2;
 *3   VEC *ovec;
***********************************************************************/
int merge_vec(VEC* vec1, VEC* vec2, VEC* ovec)
{
    register CON_WT *vec1_ptr, *vec1_end, *vec2_ptr, *vec2_end;
	CON_WT *start_vec1, *start_vec2, *start_ctype_conwt;
	CON_WT *conwtp;
	CON_WT *remconwt, *remconwt_end;
	long ctype;
    long *ctype_buf = (long *) NULL;
    CON_WT *conwt_buf = (CON_WT *) NULL;

    ovec->id_num = vec1->id_num;
    ovec->num_ctype = MAX(vec1->num_ctype, vec2->num_ctype);
	ovec->num_conwt = vec1->num_conwt + vec2->num_conwt;
 
    if (vec1->num_conwt != 0) {
        if (NULL == (ctype_buf = (long *)
                   malloc ((unsigned) ovec->num_ctype * sizeof (long))) ||
            NULL == (conwt_buf = (CON_WT *)
                   malloc ((unsigned) ovec->num_conwt * sizeof (CON_WT)))) 
            return (UNDEF);
    }

    ovec->ctype_len = ctype_buf;
    ovec->con_wtp = conwt_buf;

	for (	ctype = 0, start_vec1 = vec1->con_wtp, start_vec2 = vec2->con_wtp, conwtp = conwt_buf;
			ctype < ovec->num_ctype;
			start_vec1 += vec1->ctype_len[ctype], start_vec2 += vec2->ctype_len[ctype], ctype++) {
		
		// Point to the start of this ctype into the conwt array
		vec1_ptr = start_vec1;
		vec1_end = start_vec1 + vec1->ctype_len[ctype];
    	vec2_ptr = start_vec2;
    	vec2_end = start_vec2 + vec2->ctype_len[ctype];
		start_ctype_conwt = conwtp;

		// Merge each ctype (We can't merge the whole vectors at one go coz only the
		// ctype vectors are sorted by the concept numbers).
		if (vec1_ptr < vec1_end && vec2_ptr < vec2_end) {
			while (1) {
		    	if (vec1_ptr->con < vec2_ptr->con) {
    	        	conwtp->wt = vec1_ptr->wt;
	    	        conwtp->con = vec1_ptr->con;
					conwtp++;
	    	        if (++vec1_ptr >= vec1_end)
    	    	        break;
				}
        		else if (vec1_ptr->con > vec2_ptr->con){
	            	conwtp->wt = vec2_ptr->wt;
    	        	conwtp->con = vec2_ptr->con;
					conwtp++;
            		if (++vec2_ptr >= vec2_end)
                		break;
	        	}
    	    	else {          /* vec1_ptr->con == vec2_ptr->con */
        	    	conwtp->wt = vec1_ptr->wt + vec2_ptr->wt;
            		conwtp->con = vec1_ptr->con;
					conwtp++;
					++vec1_ptr; ++vec2_ptr;
    	       		if (vec1_ptr >= vec1_end || vec2_ptr >= vec2_end)
						break;
				}
			}
		}

		// Copy the rest into the output
		remconwt = vec1_ptr >= vec1_end? vec2_ptr : vec1_ptr;
		remconwt_end = vec1_ptr >= vec1_end? vec2_end : vec1_end;

		while (remconwt < remconwt_end) {
	       	conwtp->wt = remconwt->wt;
           	conwtp->con = remconwt->con;
			conwtp++; remconwt++;
		}

		ctype_buf[ctype] = conwtp - start_ctype_conwt; 
	}
	ovec->num_conwt = conwtp - conwt_buf; 

    return (1);
}

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Copy internal arrays of vec_list to ensure they are not overwritten
 *2 save_vec_list (vec_list)
 *3   VEC_LIST *vec_list;
 *4 free_vec_list (vec_list)
 *7 Save_vec_list saves a vector list for future use (remember that the 
 *7 contents of internal arrays of an object are not guaranteed to exist
 *7 past the next invocation of the procedure that produced them).
 *7 free_vec_list frees a vector_list that was previously saved.
 *8 Malloc space for both top value and internal arrays of each
 *8 vector in vec_list.
***********************************************************************/
int
save_vec_list (vec_list)
VEC_LIST *vec_list;
{
    long i;
    VEC *vec_buf;

    if (NULL == (vec_buf = (VEC *)
                 malloc ((unsigned) vec_list->num_vec * sizeof (VEC))))
        return (UNDEF);

    bcopy ((char *) vec_list->vec,
           (char *) vec_buf,
           (int) vec_list->num_vec * sizeof (VEC));

    vec_list->vec = vec_buf;

    for (i = 0; i < vec_list->num_vec; i++)
        if (UNDEF == save_vec (&vec_list->vec[i]))
            return (UNDEF);

    return (1);
}


int
free_vec_list (vec_list)
VEC_LIST *vec_list;
{
    long i;
    for (i = 0; i < vec_list->num_vec; i++)
        if (UNDEF == free_vec (&vec_list->vec[i]))
            return (UNDEF);
    (void) free ((char *) vec_list->vec);

    return (1);
}

/* Searches a concept in a vector. This is required while expanding
 * a vector. The function returns the place in the vector
 * where the new concept could be inserted. To be more
 * precise, returns the smallest integer larger than or equal to key  */
CON_WT* searchCon(CON_WT* keyconwt, CON_WT* conwt, long num_conwt) {
	long low = 0, high = num_conwt-1;
	long mid;
	int c;

	while (low < high) {
		if (keyconwt->con < conwt[low].con)
			return &conwt[low];
		if (keyconwt->con > conwt[high].con)
			return &conwt[high+1];
		mid = (low + high) >> 1;
		if (keyconwt->con == conwt[mid].con)
			return &conwt[mid];
		if (keyconwt->con < conwt[mid].con) {
			high = mid;	
		}
		else {
			low = mid+1;
		}
	}
	return &conwt[low];
}

