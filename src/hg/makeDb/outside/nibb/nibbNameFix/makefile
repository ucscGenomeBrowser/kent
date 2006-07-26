include ../../../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = nibbNameFix.o

nibbNameFix: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/nibbNameFix $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/nibbNameFix

clean:
	rm -f $O
