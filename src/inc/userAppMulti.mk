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
%_default: sharedObjectsBuilt
	${MAKE} default PROG=$*
compile:: ${PROGS:%=%_compile}
%_compile: sharedObjectsBuilt
	${MAKE} compile PROG=$*
clean:: ${PROGS:%=%_clean}
%_clean::
	${MAKE} clean PROG=$*
install: ${PROGS:%=%_}
%_install:
	${MAKE} install PROG=$*
cgi::  ${PROGS:%=%_cgi}
%_cgi: sharedObjectsBuilt
	${MAKE} cgi PROG=$*
alpha:: ${PROGS:%=%_alpha}
%_alpha: sharedObjectsBuilt
	${MAKE} alpha PROG=$*
beta::  ${PROGS:%=%_beta}
%_beta: sharedObjectsBuilt
	${MAKE} beta PROG=$*

# When the programs share a compiled object (an extraObjects entry used by more
# than one program), build it once here before the per-program sub-makes fan
# out. Otherwise parallel make lets two sub-makes compile the same object at
# once, and one truncates the .o while the other links it.
# Set sharedObjects in the PROGS branch of the directory makefile.
sharedObjectsBuilt:
ifneq (${sharedObjects},)
	${MAKE} ${sharedObjects} PROG=$(firstword ${PROGS})
endif

# fake name useds to get pass recurise ifeq
test::
	${MAKE} test PROG=testing
