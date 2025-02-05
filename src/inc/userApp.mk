########################################################################
# user App rules, typical three-line makefile to use this rule set,
#
#   the one or more program file name is specified by the 'A' variable:
#	kentSrc = ../..
#	A = aveCols
#	include ${kentSrc}/inc/userApp.mk
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
include ${kentSrc}/inc/localEnvironment.mk
include ${kentSrc}/inc/common.mk

# with SEMI_STATIC, this makes sure only allow shared lirbaries are used
userAppsCheckLinking=${kentSrc}/utils/qa/weeklybld/userAppsCheckLinking

DEPLIBS = ${preMyLibs} ${kentSrc}/lib/${MACHTYPE}/jkweb.a
ifeq ($(findstring src/hg/,${CURDIR}),src/hg/)
  DEPLIBS = ${preMyLibs} ${kentSrc}/lib/${MACHTYPE}/jkhgap.a ${kentSrc}/lib/${MACHTYPE}/jkweb.a
endif

LINKLIBS = ${STATIC_PRE} ${DEPLIBS} ${MYSQLLIBS}

objects = ${extraObjects} ${externObjects}

default:: ${A:%=${DESTDIR}${BINDIR}/%${EXE}}

${extraObjects}: ${extraHeaders}

${DESTDIR}${BINDIR}/%${EXE}: %.o ${objects} ${DEPLIBS}
	@mkdir -p $(dir $@)
	${CC} ${COPT} -o $@ $*.o ${objects} ${LINKLIBS} ${L}
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}
ifeq (${SEMI_STATIC},yes)
	${userAppsCheckLinking} $@
endif

compile:: ${A:%=%${EXE}}

%${EXE}: ${DEPLIBS} %.o ${objects}
	${CC} ${COPT} -o $@ $*.o ${objects} ${LINKLIBS} ${L}

install:: ${A:%=${DESTDIR}${BINDIR}/%${EXE}}

clean::
	rm -f ${O} ${extraObjects} ${A:%=%${EXE}}
	@if test -d tests -a -s tests/makefile; then cd tests && ${MAKE} clean; fi

test::
	@if test -d tests -a -s tests/makefile; then (cd tests && ${MAKE} test); \
	else echo "# no tests directory (or perhaps no tests/makefile) in $(CURDIR)"; fi

${extraObjects}: ${extraHeaders}
