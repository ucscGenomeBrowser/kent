include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = twoBitToFa.o

twoBitToFa: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/twoBitToFa $O ${MYLIBS} $L
#	strip ${HOME}/bin/${MACHTYPE}/twoBitToFa

clean:
	rm -f $O
