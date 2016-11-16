########################################################################
# user App rules, typical three-line makefile to use this rule set,
#   the binary program file name is specified by the 'A' variable:
#	kentSrc = ../..
#	A = aveCols
#	include ${kentSrc}/inc/userApp.mk
#
# for more than one object file for the resulting 'A' program, use
#       extraObjects = second.o third.o fourth.o etc.o
#
# to use object files built elsewhere:
#       externObjects = ../path/other.o
#
# use other libraries BEFORE jkweb.a
#     preMyLibs += path/to/lib/other.a
#
include ${kentSrc}/inc/localEnvironment.mk
include ${kentSrc}/inc/common.mk

DEPLIBS = ${preMyLibs} ${kentSrc}/lib/${MACHTYPE}/jkweb.a
ifeq ($(findstring src/hg/,${CURDIR}),src/hg/)
  DEPLIBS = ${preMyLibs} ${kentSrc}/lib/${MACHTYPE}/jkhgap.a ${kentSrc}/lib/${MACHTYPE}/jkweb.a
endif

LINKLIBS = ${DEPLIBS} ${MYSQLLIBS}

O = ${A}.o
objects = ${O} ${extraObjects} ${externObjects}

${DESTDIR}${BINDIR}/${A}${EXE}: ${DEPLIBS} ${O} ${extraObjects}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A}${EXE} ${objects} ${LINKLIBS} ${L}
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${DEPLIBS} ${O} ${extraObjects}
	${CC} ${COPT} ${CFLAGS} -o ${A}${EXE} ${objects} ${LINKLIBS} ${L}

install:: compile
	rm -f ${DESTDIR}${BINDIR}/${A}${EXE}
	cp -p ${A}${EXE} ${DESTDIR}${BINDIR}/${A}${EXE}
	${STRIP} ${A}${EXE} ${DESTDIR}${BINDIR}/${A}${EXE}
	rm -f ${O} ${A}${EXE}

clean::
	rm -f ${O} ${extraObjects} ${A}${EXE}
	@if test -d tests -a -s tests/makefile; then cd tests && ${MAKE} clean; fi

test::
	@if test -d tests -a -s tests/makefile; then (cd tests && ${MAKE} test); \
	else echo "# no tests directory (or perhaps no tests/makefile) in $(CURDIR)"; fi
