include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = orfEnum.o

orfEnum: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/orfEnum $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/orfEnum${EXE}

test:
	orfEnum in.fa out.orf
	diff expected.orf out.orf
	orfEnum -various in.fa outVarious.orf
	diff expectedVarious.orf outVarious.orf

clean:
	rm -f $O
