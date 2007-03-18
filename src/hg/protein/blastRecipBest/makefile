include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = blastRecipBest.o

blastRecipBest: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/blastRecipBest $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/blastRecipBest${EXE}

test:
	blastRecipBest inA.tab inB.tab outA.tab outB.tab
	diff expectedA.tab outA.tab
	diff expectedB.tab outB.tab

clean:
	rm -f $O
