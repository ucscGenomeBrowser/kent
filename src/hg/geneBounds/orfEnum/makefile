include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = orfEnum.o

orfEnum: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/orfEnum $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/orfEnum${EXE}

test:	orfEnum
	orfEnum in.fa out.orf
	diff expected.orf out.orf
	orfEnum -various in.fa outVarious.orf
	diff expectedVarious.orf outVarious.orf

clean:
	rm -f ${O}
