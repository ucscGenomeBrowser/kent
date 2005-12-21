include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = testSearch.o

testSearch: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/testSearch $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/testSearch

clean:
	rm -f $O
