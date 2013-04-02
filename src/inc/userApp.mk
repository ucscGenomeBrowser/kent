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
include ${kentSrc}/inc/common.mk

MYLIBS = ${preMyLibs} ${kentSrc}/lib/${MACHTYPE}/jkweb.a
ifeq ($(findstring src/hg/,${CURDIR}),src/hg/)
  MYLIBS = ${preMyLibs} ${kentSrc}/lib/${MACHTYPE}/jkhgap.a ${kentSrc}/lib/${MACHTYPE}/jkweb.a ${MYSQLLIBS} -lm
endif

O = ${A}.o
objects = ${O} ${extraObjects} ${externObjects}

all ${A}: ${O} ${extraObjects}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A} ${objects} ${MYLIBS} ${L}
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:: ${O} ${extraObjects} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${A}${EXE} ${objects} ${MYLIBS} ${L}

install:: compile
	rm -f ${DESTDIR}${BINDIR}/${A}${EXE}
	cp -p ${A}${EXE} ${DESTDIR}${BINDIR}/${A}${EXE}
	${STRIP} ${A}${EXE} ${DESTDIR}${BINDIR}/${A}${EXE}
	rm -f ${O} ${A}${EXE}

clean::
	rm -f ${O} ${extraObjects} ${A}${EXE}
