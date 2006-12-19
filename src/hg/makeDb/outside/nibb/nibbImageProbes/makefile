include ../../../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = nibbImageProbes.o

nibbImageProbes: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/nibbImageProbes $O ${MYLIBS} $L
	strip ${BINDIR}/nibbImageProbes

clean:
	rm -f $O
