include ../../inc/common.mk


L = -lm
MYLIBDIR = ${HOME}/src/lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = makeOrf2gene.o
A = makeOrf2gene

makeOrf2gene: $O ${MYLIBS}
	${CC} -o ${HOME}/bin/${MACHTYPE}/$A $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/$A${EXE}

clean:
	rm -f ${O}
