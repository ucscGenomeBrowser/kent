include ../../../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = nibbNameFix.o

nibbNameFix: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/nibbNameFix $O ${MYLIBS} $L
	strip ${BINDIR}/nibbNameFix

clean:
	rm -f $O
