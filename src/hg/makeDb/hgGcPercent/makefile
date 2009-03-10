include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgGcPercent.o 

hgGcPercent: $O ../../../lib/${MACHTYPE}/jkweb.a
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgGcPercent \
		$O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgGcPercent${EXE}

compile: $O ../../../lib/${MACHTYPE}/jkweb.a
	${CC} ${COPT} ${CFLAGS} -o hgGcPercent ${O} ${MYLIBS} $L

lib:
	cd ../../../lib && ${MAKE}

clean:
	rm -f ${O} hgGcPercent
