kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS = $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

O = qaAgpToQacIdx.o

qaAgpToQacIdx: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/qaAgpToQacIdx $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/qaAgpToQacIdx${EXE}

clean:
	rm -f $O
