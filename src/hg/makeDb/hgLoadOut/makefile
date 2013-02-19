include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgLoadOut.o

hgLoadOut: $O ../../../lib/${MACHTYPE}/jkweb.a
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgLoadOut $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgLoadOut${EXE}

lib:
	cd ../../../lib && ${MAKE}

install::
	rm -f ${SCRIPTS}/countRmsk
	cp -p ./countRmsk.pl ${SCRIPTS}/countRmsk

compile:: $O
	${CC} ${COPT} ${CFLAGS} -o hgLoadOut $O ${MYLIBS} $L

clean::
	rm -f ${O} hgLoadOut
