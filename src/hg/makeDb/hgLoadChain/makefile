include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgLoadChain.o 

hgLoadChain: $O ../../../lib/${MACHTYPE}/jkweb.a
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgLoadChain \
		$O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgLoadChain${EXE}

lib:
	cd ../../../lib && ${MAKE}

clean::
	rm -f ${O}
