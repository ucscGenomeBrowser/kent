kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = gsidGetConsensus.o

gsidGetConsensus: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/gsidGetConsensus $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/gsidGetConsensus${EXE}

test: test1 
%:	input/%
	${MKDIR} output/$@
	gsidGetConsensus input/$@ output/$@
	diff -x CVS -r expected/$@ output/$@
	rm -r output

clean::
	rm -f ${O}
