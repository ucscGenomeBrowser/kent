include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = scrapeCruzBiotech.o

scrapeCruzBiotech: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/scrapeCruzBiotech $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/scrapeCruzBiotech${EXE}

clean:
	rm -f $O
