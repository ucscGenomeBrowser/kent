include ../../../inc/common.mk

HG_WARN=-Wall

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgCalStat.o

hgCalStat: $O ../../../lib/${MACHTYPE}/jkweb.a
	${CC} ${COPT} ${CFLAGS} -o ${HOME}/bin/${MACHTYPE}/hgCalStat $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/hgCalStat${EXE}

lib:
	cd ../../../lib && ${MAKE}

install::
	cp -p ./stats.pl ${SCRIPTS}

clean::
	rm -f ${O} hgCalStat
