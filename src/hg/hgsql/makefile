include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgsql.o

hgsql: $O ${MYLIBS}
	${CC} -o ${HOME}/bin/${MACHTYPE}/hgsql $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/hgsql${EXE}

clean::
	rm -f ${O}
