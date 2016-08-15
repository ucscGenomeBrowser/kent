kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = raToSvmLight.o

raToSvmLight: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/raToSvmLight $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/raToSvmLight${EXE}

test:
	raToSvmLight in.ra name out.feature out.keys -good -fields=out.fields
	diff expected.feature out.feature
	diff expected.keys out.keys
	diff expected.fields out.fields

clean:
	rm -f $O
