#               LIBRARY MAKEFILE

# Module name
LIBNAME = liblconvert

WEIGHT   = weights_lnorm.o weight_slope.o weight_len.o \
                weights_tf.o weights_idf.o weight_okapi.o \
		weight_inquery.o weight_log.o \
		weights_normu.o weight_fuzzy.o weight_lm.o 

LOW_LEVEL = contok_di_ni.o contok_beng.o con_cw_ictf.o lda_gibbs.o bilda.o ilda.o
MID_LEVEL = trec_tr_o.o trec_qrels_o.o tr_trec_o.o \
		tr_inex_o.o inex_qrels_o.o \
		tr_elementlist_o.o tr_wt_elementlist_o.o \
		trec_qrtr_o.o tr_prob_o.o rr_prob_o.o vecwt_vec_o.o \
		create_cent.o did_convec.o vec_cooccvec_o.o \
		tr_tr_rewt.o tr_tr_pnorm.o \
		tr_tr_cooc_li.o \
		tr_tr_nnsim.o \
		tr_tr_cl_ns.o tr_tr_cl_sa.o \
		tr_tr_cl_ret.o tr_tr_itrlm.o \
		q_qtc.o vec_corr_o.o destem_o.o \
		text_fuzzy_vec.o tr_sbqe.o tr_sbqr_vec_o.o tr_sbqr_incr_vec_o.o tr_redqvec_o.o tr_tr_fusion.o \
		tr_ldainput.o tr_tr_rlm.o tr_tr_trlm.o tr_tr_trlm_clir.o tr_tr_trlm_clir_dict.o tr_tr_trlm_clir_bilda.o \
		tr_tr_relrerank.o text_bidict.o tr_tr_rlm_qexp.o tr_tr_trlm_qexp.o tr_tr_editdist.o tr_tr_trlm_phrase.o tr_tr_trlm_inter.o
#		cluster_eids.o tr_tr_coocc.o 
TOP_LEVEL = did_convec_o.o text_synphr_o.o \
		vec_filtervec.o q_qtc_o.o text_ltrie.o text_boolvec_o.o text_tiledvec_o.o text_inexfolsub.o \
		text_inex_fcs_to_ric.o tr_sbqe_vec_o.o text_vec_o.o tr_inexqa.o
LIBFILES  = $(LOW_LEVEL) $(MID_LEVEL) $(TOP_LEVEL) $(WEIGHT)

#############################################################################
#  All code below this line (except for automatically created dependencies)
#  is independent of this particular makefile, and should not be changed!
#############################################################################

# Flags normally set in top level makefile.  Make sure you want these
# default if you compile locally in this directory
PREFIX = /mnt/sdb2/smart/src
SRCDIR = /mnt/sdb2/smart/src
BIN = $(PREFIX)/bin
LIB = $(PREFIX)/lib
H   = -I$(SRCDIR)/h -I$(SRCDIR)/liblocal/h
LINTH  = $(SRCDIR)/h


# Static libraries with gcc
CC       = gcc
CFLAGS   = $H -L$(LIB) -O -gstabs+ -Wall
CDFLAGS  = -MM

# Static libraries with cc
#ccstat CC       = cc
#ccstat CFLAGS   = $H -L$(LIB) -g -Bstatic

# Dynamic libraries with Sun's unbundled cc
#cc CC       = cc
#cc CFLAGS   = $H -L$(LIB) -g
#cc CDFLAGS  = -Xm1
#cc OPTFLAGS = -O4
#cc STATIC   =


# Other macros used in some or all makefiles
PRINT  = print
AR     = -ar
ARFLAGS= ruv
#RANLIB = ranlib
RANLIB = echo

# Entry points install and all are the same.  If you want to "make install"
# a non-test version in this directory alone, YOU MUST SPECIFY "PREFIX=..." ON
# THE COMMAND LINE  where ".../lib" gives the location you wish the
# library placed.  By default it will end up in the test directory.

install all:  $(LIB)/$(LIBNAME).a


#########################################################################
# Compilation of Libraries                                              #
# Note that a copy of the library is NOT kept in the current directory. #
#########################################################################
$(LIB)/$(LIBNAME).a : $(LIB)/$(LIBNAME).a($(LIBFILES))
	$(RANLIB) $(LIB)/$(LIBNAME).a

#libnotsun $(LIB)/$(LIBNAME).a : $(LIBFILES)
#libnotsun 	$(AR) $(ARFLAGS) $(LIB)/$(LIBNAME).a $?
#libnotsun 	$(RM) $?
#libnotsun 	$(RANLIB) $(LIB)/$(LIBNAME).a


#########################################################################
# Lint                                                                  #
#########################################################################
# This invocation of lint is convoluted but semi-useful. Note that many
# routines are never explicitly called by others, but instead called
# through procedure tables.  "makelint" has to be run on all the libraries
# before "lint" will produce valid results.
makelint: $(LIB)/llib-l$(LIBNAME).ln

$(LIB)/llib-l$(LIBNAME).ln: *.c
	$(LINT) $H -C$(LIBNAME) *.c
	mv llib-l$(LIBNAME).ln $(LIB)

lint: *.c
	for f in $? ;\
	   do echo $$f; $(LINT) -v $H $$f $(LIB)/llib* -lm\
		| sed -e /multiply/d -e /errno/d -e /proc_root_root/d;\
	done    
	touch lint


#########################################################################
# Odds and ends                                                         #
#########################################################################
clean:
	/bin/rm -f *.o *.BAK *~  lint $(LIB)/llib-l$(LIBNAME).ln \
                  $(LIB)/$(LIBNAME).a makefile.old

semiclean:
	/bin/rm -f *.o

#########################################################################
# Determining program dependencies                                      #
#########################################################################
depend:
	-$(CC) $H $(CDFLAG) *.c | \
	sed -e 's?\(.*\.o\):?\$$(LIB)/\$$(LIBNAME)\.a(\1):?' \
	> makedep
	echo '/^# DO NOT DELETE THIS LINE/+2,$$d' >eddep
	echo '$$r makedep' >>eddep
	echo 'w' >>eddep
	cp makefile makefile.bak
	ed - makefile < eddep
	/bin/rm eddep makedep
	echo '# DEPENDENCIES MUST END AT END OF FILE' >> makefile
	echo '# IF YOU PUT STUFF HERE IT WILL GO AWAY' >> makefile
	echo '# see make depend above' >> makefile

# DO NOT DELETE THIS LINE -- make depend uses it

# DEPENDENCIES MUST END AT END OF FILE
# IF YOU PUT STUFF HERE IT WILL GO AWAY
# see make depend above

