kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = blastRecipBest.o

blastRecipBest: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/blastRecipBest $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/blastRecipBest${EXE}

test:
	blastRecipBest inA.tab inB.tab outA.tab outB.tab
	diff expectedA.tab outA.tab
	diff expectedB.tab outB.tab

clean:
	rm -f $O
