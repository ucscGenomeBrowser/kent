kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgCalStat.o

hgCalStat: $O ../../../lib/${MACHTYPE}/jkweb.a
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgCalStat $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgCalStat${EXE}

lib:
	cd ../../../lib && ${MAKE}

install::
	rm -f ${SCRIPTS}/stats.pl
	cp -p ./stats.pl ${SCRIPTS}

clean::
	rm -f ${O} hgCalStat
