include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgsqladmin.o

hgsqladmin: $O ${MYLIBS}
	${CC} -o ${HOME}/bin/${MACHTYPE}/hgsqladmin $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/hgsqladmin${EXE}

clean::
	rm -f ${O}
