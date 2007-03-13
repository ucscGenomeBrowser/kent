include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = raToSvmLight.o

raToSvmLight: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/raToSvmLight $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/raToSvmLight${EXE}

test:
	raToSvmLight in.ra name out.feature out.keys -good -fields=out.fields
	diff expected.feature out.feature
	diff expected.keys out.keys
	diff expected.fields out.fields

clean:
	rm -f $O
