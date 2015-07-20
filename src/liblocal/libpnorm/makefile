#               LIBRARY MAKEFILE
########################################################

# Module name
LIBNAME = liblpnorm

# Files for this particular library module
TOP = pnorm_coll.o
PREPARSE = pp_pnorm.o ppre_scan.o
INDEX = pindex_pp.o pscan.o pparser.o fix_up.o make_vector.o
PCONWT = pconvert_wt.o
CHANGEP = change_p.o
VEC_TO_BOOL = vec_to_bool_and.o vec_to_bool_or.o

OBJ_FILE  = pnorm_vec.o

PRINT_P = p_pnorm_vec.o pobj_pnorm_vec.o

RETRIEVE = get_q_pvec.o inv_eval_t.o inv_eval_p.o p_eval.o sim_cty_inv_p.o sim_vec_inv_p.o t_op.o

VEC_VEC = sim_pvec.o sim_pvec_pvec.o

CONVERT = best_pqvec_o.o

firstentry: install ppre_scan.c pscan.c pparser.c

ppre_scan.c: ppre_scan.l ppre_scan.sed
	lex -t ppre_scan.l | sed -f ppre_scan.sed > ppre_scan.c

#       lex -t pscan.l > pscan.c
#       yacc pparser.y
#       mv y.tab.c pparser.c

delete_tmpfile:
	rm -f ppre_scan.c pparser.c pscan.c

LIBFILES  = $(VEC_TO_BOOL) $(TOP) $(PREPARSE) $(INDEX) $(PCONWT) $(CHANGEP) \
	    $(RETRIEVE) $(VEC_VEC) $(CONVERT) \
	    $(PRINT_P) \
	    $(OBJ_FILE)

#  All code below this line (except for automatically created dependencies)
#  is independent of this particular makefile, and should not be changed!
#############################################################################

# Flags normally set in top level makefile.  Make sure you want these
# default if you compile locally in this directory
PREFIX = /home/mandar/smart/src
SRCDIR = /home/mandar/smart/src
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

