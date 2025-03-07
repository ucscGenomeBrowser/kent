kentSrc = ../../..
.NOTPARALLEL:  # prevent two ntContig.o compiles at once

# recursive make uglyness (see userArp.mk)
ifeq (${PROG},)
PROGS = hgGoldGapGl hgLoadGap

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

else
A = ${PROG}
extraHeaders = ntContig.h
extraObjects = ntContig.o
include ${kentSrc}/inc/userApp.mk

endif
