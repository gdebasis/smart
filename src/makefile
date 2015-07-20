###########################################################################
#                            THE
#                           SMART
#               INFORMATION RETRIEVAL SYSTEM
#
#   (c) copyright 1984,1988, 1989, 1991, 1992 Chris Buckley, Gerard Salton
#
#   This recursive makefile constructs all programs needed by the 
#   SMART system and places them in ../bin.
#
#   The main entry point for compilation is "install".
#   Each makefile in a source sub-directory has (at least) six
#   entry points:
#       all:     compile local copies of files
#       install: in addition, compile global copies of programs and libraries
#       clean:   remove all unnecessary files from directory
#       print:   print a hardcopy of all text (doesn't work (rcs))
#       depend:  change the makefile to establish all dependencies of source
#                files on .h files
#
#   This makefile has entry points corresponding to the six above, plus
#   an entry point to produce installed code that is profiled, and
#   another to produce installed optimized code.
############################################################################

ACTION   = all

SRCDIR = /mnt/sdb2/smart/src
PREFIX = /mnt/sdb2/smart/src

# liblocal is defined to be a duplicate hierarchy for local use.
# All local (non-Cornell) procedure versions should be kept in liblocal
# If you have other local hierarchies you want linked, append to LOCALHIER.
LOCALHIER = liblocal

BIN = $(PREFIX)/bin
LIB = $(PREFIX)/lib
H   = -I$(SRCDIR)/h -I$(SRCDIR)/liblocal/h 

# Version numbers
VERSION = 12.0

# Sun static linking with cc
#ccstat CC     = cc
#ccstat STATIC = -Bstatic
#ccstat CFLAGS =  -L$$(LIB) $H $(STATIC)
#ccstat OPTFLAGS = -O4 -fsingle


# Static libraries with gcc
# Possible Flags for gcc 
CC       = gcc
CFLAGS   = $H -L$$(LIB) -Wall
OPTFLAGS = -O2 -finline-functions 
STATIC   = -static

# Dynamic libraries with Sun's unbundled cc
#cc CC       = cc
#cc CFLAGS   = $H -L$$(LIB)
#cc OPTFLAGS = -fast
#cc STATIC   = 


# Default action
default: 
	-mkdir $(PREFIX)
	-mkdir $(PREFIX)/bin
	-mkdir $(PREFIX)/lib
	make  ACTION=all 'CFLAGS=$(CFLAGS) -gstabs+' lib_action program_action

everything: default optimize profile

install: 
	make clean
	make  ACTION=install "BIN=$(INSTALL_BIN)" lib_action program_action

clean:
	make  ACTION=clean h_action lib_action program_action

semiclean:
#libnotsun semiclean:
#libnotsun 	make  ACTION=semiclean lib_action program_action

depend:
	make  ACTION=depend lib_action program_action

profile:
	-mkdir $(PREFIX)/dirprofile
	-mkdir $(PREFIX)/dirprofile/bin
	-mkdir $(PREFIX)/dirprofile/lib
	make semiclean
	make  ACTION=all 'CFLAGS=$(CFLAGS) -pg $(OPTFLAGS) $(STATIC)' \
              PREFIX=$(PREFIX)/dirprofile lib_action program_action
	make semiclean

optimize:
	-mkdir $(PREFIX)/diropt
	-mkdir $(PREFIX)/diropt/bin
	-mkdir $(PREFIX)/diropt/lib
	make semiclean
	make  ACTION=all 'CFLAGS=$(CFLAGS) $(OPTFLAGS)' \
              PREFIX=$(PREFIX)/diropt  lib_action program_action
	make semiclean

# WARNING: Whether tcov will work, and with what flags, is compiler dependent
counted:  
	-mkdir $(PREFIX)/dircount
	-mkdir $(PREFIX)/dircount/bin
	-mkdir $(PREFIX)/dircount/lib
	make semiclean
	make  ACTION=all 'CFLAGS=$(CFLAGS) -a' \
              PREFIX=$(PREFIX)/dircount  lib_action program_action
	make semiclean

# WARNING: Whether ansi will work, and with what flags, is compiler dependent
ansi:  
	-mkdir $(PREFIX)/diransi
	-mkdir $(PREFIX)/diransi/bin
	-mkdir $(PREFIX)/diransi/lib
	make semiclean
	make  ACTION=all 'CFLAGS=$H -L$(LIB) -g -ansi -Wall' \
              PREFIX=$(PREFIX)/diransi  lib_action program_action

dec:  
	-mkdir $(PREFIX)/dirdec
	-mkdir $(PREFIX)/dirdec/bin
	-mkdir $(PREFIX)/dirdec/lib
	make semiclean
	make  ACTION=all 'CFLAGS=$(CFLAGS) -a' \
              PREFIX=$(PREFIX)/dirdec  lib_action program_action

lint:
	-mkdir $(PREFIX)
	-mkdir $(PREFIX)/bin
	-mkdir $(PREFIX)/lib
	make  ACTION=makelint lib_action
	make  ACTION=lint lib_action program_action

makefiles:
	for dir in $(LIBLIST) ;\
                do awk '/All code below this line/ {exit} {print}' \
                    < $$dir/makefile > $$dir/makefile.new;\
        	cat makelib.templ >> $$dir/makefile.new;\
	        mv $$dir/makefile $$dir/makefile.old;\
        	mv $$dir/makefile.new $$dir/makefile;\
        done
	for dir in $(BINLIST) $(UTILIST) $(SHLIST); \
	        do awk '/##1##/, /##2##/ {print}' \
		    < makebin.templ > $$dir/makefile.new;\
		awk '/##3##/,/##4##/ {print}' \
		    < $$dir/makefile >> $$dir/makefile.new;\
		awk '/##5##/, /##6##/ {print}' \
		    < makebin.templ >> $$dir/makefile.new;\
	        mv $$dir/makefile $$dir/makefile.old;\
        	mv $$dir/makefile.new $$dir/makefile;\
	done
	-for dir in $(LOCALHIER) ; \
                do (cd $(LOCALHIER); make makefiles) \
        done
#	make depend

test_suite:
	(cd test_adi; ./test_adi)

LIBLIST = libconvert libevaluate libexp \
          libfeedback libfile libgeneral libindexing libinter \
          libprint libproc libretrieve
#libobsolete not included for now

UTILIST = utility
BINLIST = smart
SHLIST  = scripts
HLIST   = h

#########################################################################
# Actions on libraries                                                  #
#########################################################################
lib_action:
	for dir in $(UTILIST) $(LIBLIST) $(LOCALHIER);\
                do cd $$dir ;\
                make -k "$(ACTION)" "CFLAGS=$(CFLAGS)" PREFIX=$(PREFIX) \
                        "BIN=$(BIN)" ;\
                cd ..;\
        done

#########################################################################
# Actions on programs                                                   #
# Note that compilation depends on the desired libraries being present. #
# These libraries are made above!                                       #
#########################################################################
program_action: 
	for dir in $(BINLIST) $(SHLIST);\
                do cd $$dir ;\
                make -k "$(ACTION)" "CFLAGS=$(CFLAGS)" PREFIX=$(PREFIX) \
                        "BIN=$(BIN)" ;\
                cd ..;\
        done

#########################################################################
# Actions on include files
#########################################################################
h_action: 
	(cd h; make -k $(ACTION) )

#########################################################################
# Emacs tags file
#########################################################################
tags:
	touch TAGS
	/bin/rm -f TAGS
	touch TAGS
	for dir in h $(BINLIST) $(LIBLIST) ; do \
		etags -atf TAGS $$dir/*.[ch] ;\
	done
	-for dir in $(LOCALHIER) ; do \
		cd $$dir ; \
		make tags TAGDIR=$$dir;\
		cd .. ;\
	done
