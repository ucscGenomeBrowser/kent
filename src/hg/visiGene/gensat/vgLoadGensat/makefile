include ../../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = vgLoadGensat.o ../lib/gs.o

vgLoadGensat: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/vgLoadGensat $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/vgLoadGensat${EXE}

clean:
	rm -f $O
