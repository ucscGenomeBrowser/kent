kentSrc = ../../..
extraObjects = hooklib.o
extraHeaders = hooklib.h
CGI_LOADER_SUBDIR = hooks

ifeq (${PROG},)
PROGS = pre-create pre-finish

default:: ${PROGS:%=%_default}
%_default:
	${MAKE} default PROG=$*
compile:: ${PROGS:%=%_compile}
%_compile:
	${MAKE} compile PROG=$*
clean:: ${PROGS:%=%_clean}
%_clean:
	${MAKE} clean PROG=$*
install:: ${PROGS:%=%_install}
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
