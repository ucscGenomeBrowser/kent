include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = makeTrackIndex

O = $A.o

$A: ${O}
	@${MKDIR} "${DESTDIR}${BINDIR}"
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} ${L}
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

clean:
	rm -f $O

build:
	tmpFile=`mktemp`;\
	makeTrackIndex $$DBS > $$tmpFile;\
	rm -f /gbdb/$$DBS/trackDb.ix /gbdb/$$DBS/trackDb.ixx;\
	ixIxx $$tmpFile /gbdb/$$DBS/trackDb.ix /gbdb/$$DBS/trackDb.ixx;\
	rm -f $$tmpFile	
