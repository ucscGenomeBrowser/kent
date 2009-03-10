include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = txGeneAltProt.o

txGeneAltProt: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txGeneAltProt $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txGeneAltProt${EXE}

clean:
	rm -f $O
