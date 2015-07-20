#               LIBRARY MAKEFILE
########################################################

# Module name
LIBNAME = libevaluate

# Files for this particular library module
LOW_LEVEL = qeval.o
MID_LEVEL = rr_eval_list.o tr_eval_list.o
CONV_LEVEL = ell_el_avg.o ell_el_dev.o ell_el_compo.o ell_el_compoe.o
PRINT_LEVEL = p_eval_list.o p_s_eval_list.o p_p_eval_list.o pobj_eval.o \
	pobj_qq_eval.o pobj_ref_eval.o p_p_ell.o mrr_eval.o 

LIBFILES  = $(LOW_LEVEL) $(MID_LEVEL) $(CONV_LEVEL) $(PRINT_LEVEL) $(TOP_LEVEL)


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

