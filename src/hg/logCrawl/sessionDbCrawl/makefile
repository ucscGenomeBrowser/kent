kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = sessionDbCrawl.o

sessionDbCrawl: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/sessionDbCrawl $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/sessionDbCrawl${EXE}

clean:
	rm -f $O
