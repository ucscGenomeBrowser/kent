include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = sessionDbCrawl.o

sessionDbCrawl: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/sessionDbCrawl $O ${MYLIBS} $L
	strip ${BINDIR}/sessionDbCrawl

clean:
	rm -f $O
