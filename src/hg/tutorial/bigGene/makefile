kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

O = bigGene.o

bigGene: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/bigGene $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/bigGene${EXE}

clean:
	rm -f $O
