include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgLoadChain.o 

hgLoadChain: $O ../../../lib/${MACHTYPE}/jkweb.a
	${CC} ${COPT} ${CFLAGS} -o ${HOME}/bin/${MACHTYPE}/hgLoadChain \
		$O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/hgLoadChain${EXE}

lib:
	cd ../../../lib; make

clean::
	rm -f ${O}
