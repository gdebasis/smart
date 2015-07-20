#               LIBRARY MAKEFILE
########################################################

# Module name
LIBNAME = libconvert

# Files for this particular library module
WEIGHT   =	weight_std.o weight_sep.o weight_no.o \
			weight_tri.o weight_tfidf.o \
			weights_normu.o weights_normc.o \
			weights_tf.o weights_idf.o weights_norm.o \
			weight_lm.o weight_lm_cf.o weight_lm_cf_nsim.o weight_lm_lda.o \

# weight_sum.o weight_slope.o weights_aidf.o weight_cos.o weight_unique.o 
UTIL     = res_tr.o sectid_num.o

LOW_LEVEL = text_dict.o vec_inv.o vec_invpos.o did_vec.o \
            contok_dict.o \
            con_inv.o con_cw_idf.o con_cw_cf.o \
            con_invidf.o con_invfreq.o 
#            pp_veclist.o  pp_pp_parts.o pp_pp_win.o pp_vl_wv.o
#       con_cw_pidf.o con_invpidf.o 	    id_textdoc.o
MID_LEVEL = text_dict_o.o vec_inv_o.o vec_aux_o.o text_rr_o.o text_tr_o.o \
            tr_rel_o.o vec_vec_o.o inv_inv_o.o tloc_tloc_o.o vecwt_aux_o.o \
            text_vec_o.o weight_o.o merge_vec_files.o reduce_vec_lmscore.o \
			weight_exp_o.o docexp_lm.o docexp_rlm.o docexp_trlm.o docexp_lda.o \
			vecexp_dcomp.o weight_lda_o.o
			#vecexp_dcomp_bn.o vecexp_dcomp_bn_cocc.o vecexp_dcomp_hn.o vecexp_dcomp_hn_cocc.o
TOP_LEVEL = convert.o
LIBFILES  = $(WEIGHT) $(VECINV) $(DIDVEC) $(UTIL) \
            $(LOW_LEVEL) $(MID_LEVEL) $(TOP_LEVEL)


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

