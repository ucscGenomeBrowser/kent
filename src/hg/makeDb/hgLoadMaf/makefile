kentSrc = ../../..
# recursive make uglyness (see userArp.mk)
ifeq (${PROG},)
PROGS = hgLoadMaf hgLoadMafSummary

default:: ${PROGS:%=%_default}
%_default:
	${MAKE} default PROG=$*
compile:: ${PROGS:%=%_complie}
%_compile:
	${MAKE} compile PROG=$*
clean:: ${PROGS:%=%_clean}
%_clean::
	${MAKE} clean PROG=$*
install: ${PROGS:%=%_}
%_install:
	${MAKE} install PROG=$*

cgi:: cgi_any
alpha:: cgi_any
beta:: cgi_any
cgi_any:: ${PROGS:%=%_cgi_any}
%_cgi_any:
	${MAKE} cgi_any PROG=$*

test::
	${MAKE} test PROG=testing
else
A = ${PROG}
include ${kentSrc}/inc/cgiLoader.mk
endif
