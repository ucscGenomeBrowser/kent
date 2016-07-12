kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = scrapeCruzBiotech.o

scrapeCruzBiotech: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/scrapeCruzBiotech $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/scrapeCruzBiotech${EXE}

clean:
	rm -f $O
