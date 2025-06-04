########################################################################
# user App rules, typical three-line makefile to use this rule set,
#
#   the one program file name is specified by the 'A' variable:
#	kentSrc = ../..
#	A = aveCols
#	include ${kentSrc}/inc/userApp.mk
#
# See userAppMulti.mk if multiple programs are required in a single directory.
#
# Other variables that can be defined, which needs to be done
# before including userApp.mk
#
# for more than one object file for the resulting 'A' program, use
#       extraObjects = second.o third.o fourth.o etc.o
#
# and for extra header files for depenencies
#       extraHeaders = second.h third.h fourth.h etc.h
#
# to use object files built elsewhere:
#       externObjects = ../path/other.o
#
# use other libraries BEFORE jkweb.a
#     preMyLibs += path/to/lib/other.a
#
# Note: Reason for restriction on compiling one program.:
#   Due to the non-standard use of make, the expected behavor is that the
#   program gets rebuilt if the .o is missing (after make clean), even if the
#   program in the destination is current with the C file.  This doesn't work
#   with make pattern rules, as it will not rebuild the target in this case.
# 
ifneq ($(words ${A}),1)
   $(error The A variable should contain only one word, found '${A}')
endif

include ${kentSrc}/inc/localEnvironment.mk
include ${kentSrc}/inc/common.mk

# with SEMI_STATIC, this makes sure only allow shared lirbaries are used
userAppsCheckLinking=${kentSrc}/utils/qa/weeklybld/userAppsCheckLinking

DEPLIBS = ${preMyLibs} ${kentSrc}/lib/${MACHTYPE}/jkweb.a
ifeq ($(findstring src/hg/,${CURDIR}),src/hg/)
  DEPLIBS = ${preMyLibs} ${kentSrc}/lib/${MACHTYPE}/jkhgap.a ${kentSrc}/lib/${MACHTYPE}/jkweb.a
endif

LINKLIBS = ${STATIC_PRE} ${DEPLIBS} ${MYSQLLIBS}

default:: ${DESTBINDIR}/${A}${EXE}
compile:: ${A}

objects = ${A}.o ${extraObjects} ${externObjects}

${extraObjects} ${A}.o: ${extraHeaders}

${DESTBINDIR}/${A}${EXE}: ${objects} ${DEPLIBS}
	@${MKDIR} $(dir $@)
	${CC} ${COPT} -o $@ ${objects} ${LINKLIBS} ${L}
	${STRIP} $@
ifeq (${SEMI_STATIC},yes)
	${userAppsCheckLinking} $@
endif

${A}${EXE}: ${objects} ${DEPLIBS}
	${CC} ${COPT} -o $@ ${objects} ${LINKLIBS} ${L}

install:: ${A:%=${DESTBINDIR}/%${EXE}}

clean::
	rm -f ${A}.o ${extraObjects} ${A:%=%${EXE}}
	@if test -d tests -a -s tests/makefile; then cd tests && ${MAKE} clean; fi

test::
	@if test -d tests -a -s tests/makefile; then (cd tests && ${MAKE} test); \
	else echo "# no tests directory (or perhaps no tests/makefile) in $(CURDIR)"; fi

${extraObjects}: ${extraHeaders}

