kentSrc = ../..

# recursive make uglyness (see userArp.mk)
ifeq (${PROG},)
PROGS = liftOver liftOverMerge

default:: ${PROGS:%=%_default}
%_default:
	${MAKE} default PROG=$*
compile:: ${PROGS:%=%_compile}
%_compile:
	${MAKE} compile PROG=$*
clean:: ${PROGS:%=%_clean}
%_clean::
	${MAKE} clean PROG=$*
install: ${PROGS:%=%_}
%_install:
	${MAKE} install PROG=$*
test::
	${MAKE} test PROG=testing
else
A = ${PROG}
include ${kentSrc}/inc/userApp.mk
endif


