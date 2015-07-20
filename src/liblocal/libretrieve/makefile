#               LIBRARY MAKEFILE

# Module name
LIBNAME = liblretrieve

# Files for this particular library module
# top_level procedures

COMBINE	   = 
# comb_wt.o comb_wtnabs.o comb_wtnorm.o

CTYPE_VEC  = sim_filter.o

VEC_VEC	   = vec_vec_match.o vec_vec_coocc.o sentsim_lm.o sentsim_rlm.o sentsim_trlm.o sentsim_trlm_phrase.o sentsim_lmtrlm.o
# trec_thresh.o trec_thresh2.o

BEST_PIECE = 
# piece_extract.o piece_segment.o piece_theme.o piece_conti.o \
#	     piece_best.o hierarchical.o

COLL_SIM   = sim_twopass.o
# trec_vec_inv.o trec_inv.o

INV_SIM	   = sim_cty_idf.o sim_cty_coocc.o sim_c_inv_h.o sim_cty_qtc.o

RANK_TR	   = 
# rank_trec_did.o

MIXED      = 
# mixed.o explode.o resfilter.o mixed_cmpr.o mixed_expw.o

TOP        = ret_seq.o ret_seen.o ret_af.o lret_af.o lret_seen.o
# ret_lseen.o ret_l2seen.o ret_l5seen.o \
#             ret_learn_s.o ret_learn_sq.o 
GETDOC     = getdoc_vec.o getdoc_wtvec.o getdoc_tloc.o

Q_TR       = q_tr_wtdoc.o q_tr_prox.o q_tr_softbool.o q_tr_coocc.o \
		q_tr_winmatch.o q_tr_cl_sa.o
#q_tr_bool.o
QDOC       = q_tr.o q_tr_m.o qloc_qdis_tok.o qdoc_index_pp.o
QDOC_I     = q_qd_index.o q_qd_store.o q_qd_cache.o
QDOC_P     = p_qdis_ret.o p_qdis_arr.o p_qdoc_info.o p_qloc_arr.o
QDOC_S     = qdis_sim_null.o qdis_sim_stat.o qd_s_wstat.o qd_s_fc.o\
             qd_s_c_1.o \
             qd_s_1.o qd_s_2.o qd_s_3.o qd_s_4.o qd_s_5.o qd_s_6.o \
             qd_s_7.o qd_s_8.o qd_s_9.o qd_s_10.o qd_s_11.o qd_s_12.o \
             qd_s_13.o qd_s_14.o

LIBFILES   = $(MIXED) $(COLL_SIM) $(INV_SIM) $(RANK_TR) $(COMBINE) $(CTYPE_VEC) \
	     $(VEC_VEC) $(BEST_PIECE) $(TOP) $(GETDOC) $(Q_TR)
#             $(QDOC) $(QDOC_I) $(QDOC_P) $(QDOC_S)

#############################################################################
#  All code below this line (except for automatically created dependencies)
#  is independent of this particular makefile, and should not be changed!
#############################################################################

# Flags normally set in top level makefile.  Make sure you want these
# default if you compile locally in this directory
PREFIX = /home/debasis/smart/src
SRCDIR = /home/debasis/smart/src
BIN = $(PREFIX)/bin
LIB = $(PREFIX)/lib
H   = -I$(SRCDIR)/h -I$(SRCDIR)/liblocal/h
LINTH  = $(SRCDIR)/h


# Static libraries with gcc
CC       = gcc
#CFLAGS   = $H -L$(LIB) -O -gstabs+ -Wall
CFLAGS   = $H -L$(LIB) -gstabs+ -Wall
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

