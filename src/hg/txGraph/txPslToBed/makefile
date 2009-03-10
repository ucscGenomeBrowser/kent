include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

O = txPslToBed.o

txPslToBed: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txPslToBed $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txPslToBed${EXE}

test:
	txPslToBed -verbose=0 -cds=input.cds input.psl /cluster/data/hg18/hg18.2bit output.bed 
#	txPslToBed -verbose=0 input.psl /cluster/data/hg18/hg18.2bit output.bed 
	diff expected.bed output.bed

clean:
	rm -f $O
