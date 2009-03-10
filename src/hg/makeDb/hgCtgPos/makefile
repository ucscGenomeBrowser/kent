include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgCtgPos.o liftSpec.o

hgCtgPos: $O ../../../lib/${MACHTYPE}/jkweb.a
	${CC} ${COPT} -o ${BINDIR}/hgCtgPos $O ${MYLIBS} $L

lib:
	cd ../../../lib && ${MAKE}

clean::
	rm -f ${O}
