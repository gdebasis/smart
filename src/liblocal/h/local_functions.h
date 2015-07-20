#ifndef LFUNCTIONSH
#define LFUNCTIONSH

/* libinter functions */
int init_show_doc_part(), show_named_doc_part(), show_named_eid(), close_show_doc_part();

/* libconvert functions */
int init_create_cent(), create_cent(), close_create_cent();

/* libprint/libadvq functions */
void print_infoneed(), print_advvec();

/* libretrieve functions */
void print_qloc_array(), print_qdis_array(), print_qdis_ret(), print_qdoc_info();

/* libpnorm/libfile functions */
int create_pnorm(), open_pnorm(), seek_pnorm(), read_pnorm(), write_pnorm(), close_pnorm();
void print_pnorm_vector();
#endif
