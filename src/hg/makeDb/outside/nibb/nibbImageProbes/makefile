include ../../../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = nibbImageProbes.o

nibbImageProbes: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/nibbImageProbes $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/nibbImageProbes

clean:
	rm -f $O
