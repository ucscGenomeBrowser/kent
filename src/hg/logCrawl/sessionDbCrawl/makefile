include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = sessionDbCrawl.o

sessionDbCrawl: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/sessionDbCrawl $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/sessionDbCrawl

clean:
	rm -f $O
