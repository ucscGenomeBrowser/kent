kentSrc = ../../..
include ../../../inc/common.mk

L = ../../../htslib/libhts.a $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

UPDATE_A = mdbUpdate
PRINT_A = mdbPrint
UPDATE_O = ${UPDATE_A}.o
PRINT_O = ${PRINT_A}.o

all: mdbUpdate mdbPrint

mdbUpdate: ${UPDATE_O} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/${UPDATE_A} ${UPDATE_O} ${MYLIBS} $L
	${STRIP} ${BINDIR}/${UPDATE_A}${EXE}

mdbPrint: ${PRINT_O} ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/${PRINT_A} ${PRINT_O} ${MYLIBS} $L
	${STRIP} ${BINDIR}/${PRINT_A}${EXE}

lib:
	cd ${MYLIBDIR} && ${MAKE}

compile: ${UPDATE_O} ${PRINT_O}  ../../../lib/${MACHTYPE}/jkweb.a
	${CC} ${COPT} -o ${UPDATE_A} ${UPDATE_O} ${MYLIBS} $L
	${CC} ${COPT} -o ${PRINT_A} ${PRINT_O} ${MYLIBS} $L

clean::
	rm -f ${UPDATE_A} ${PRINT_A} ${UPDATE_O} ${PRINT_O}

