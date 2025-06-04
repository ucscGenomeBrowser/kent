########################################################################
# Additional rules to use when build multiple programs with userApps.mk
#
# Due the way many rules do use patterns, it is necessary to recursively
# call make to build multiple user apps or CGI loaders in the same directory.
# Do this using this idiom:
#
#   kentSrc = ../..
#   ifeq $({PROGS},)
#   PROGS = one two three
#   include ${kentSrc}/inc/userAppMulti.mk
#   else
#   A = ${PROG}
#   include ${kentSrc}/inc/userApp.mk
#   endif


# Note MAKECMDGOALS is used to set variables, so
# can not any make magic to combine these names

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
cgi::  ${PROGS:%=%_cgi}
%_cgi:
	${MAKE} cgi PROG=$*
alpha:: ${PROGS:%=%_alpha}
%_alpha:
	${MAKE} alpha PROG=$*
beta::  ${PROGS:%=%_beta}
%_beta:
	${MAKE} beta PROG=$*

# fake name useds to get pass recurise ifeq
test::
	${MAKE} test PROG=testing
