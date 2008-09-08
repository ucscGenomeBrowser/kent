# Build all directories in utils

include ../../inc/common.mk

all::    utils

utils:
	@for D in $(USE_DIRS) x; do \
            if test "$$D" != "x" ; then \
                ( cd $$D && echo $$D && $(MAKE) ) ;\
                x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
            fi ;\
	done

# List of directories to build
USE_DIRS = $(DIRS)

clean: 
	@for D in ${USE_DIRS}; do \
		(cd $${D} && ${MAKE} clean;) \
	done

# no makefile for:
#       pfamXref
#	spXref2


DIRS = \
	gsidSubj \
	gsidTable \
	gsidMember \ 
	gsIdXref \
	gsidAaMsa \
	gsidGetConsensus \
	gsidMsa \
	gsidMsaToTab \
	gsidPbGateway \
	pbGsid \
	pwdtest

