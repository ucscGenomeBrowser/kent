include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = weedLines.o

weedLines: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/weedLines $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/weedLines${EXE}

test:
	weedLines input.weed input.txt out.good -weeded=out.bad
	diff expected.good out.good
	diff expected.bad out.bad
	
clean:
	rm -f $O
