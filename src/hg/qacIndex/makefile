kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

O = qacIndex.o

qacIndex: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/qacIndex $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/qacIndex${EXE}

clean:
	rm -f $O
