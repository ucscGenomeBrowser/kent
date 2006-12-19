include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

PROG1 = hgLoadMaf
PROG2 = hgLoadMafSummary

all: ${PROG1} ${PROG2}

${PROG1}: ${PROG1}.o $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/${PROG1} ${PROG1}.o $(MYLIBS) $L
#${STRIP} ${BINDIR}/${PROG1}${EXE}

${PROG2}: ${PROG2}.o $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/${PROG2} ${PROG2}.o $(MYLIBS) $L
#${STRIP} ${BINDIR}/${PROG2}${EXE}

clean::
	rm -f ${PROG1}.o ${PROG2}.o $B


