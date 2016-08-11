kentSrc = ../../..
include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
MYFILEROOT=trackDb

A = makeTrackIndex

O = $A.o

$A: ${O}
	@${MKDIR} "${DESTDIR}${BINDIR}"
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} ${L}
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

alpha:
	${MAKE} doBuild

beta:
	${MAKE} doBuild MYFILEROOT=trackDb_beta

clean:
	rm -f $O

doBuild:
	tmpFile=`mktemp`;\
	makeTrackIndex $(DBS) metaDb /usr/local/apache/cgi-bin/encode/cv.ra > $$tmpFile;\
	rm -f /gbdb/$(DBS)/$(MYFILEROOT).ix /gbdb/$(DBS)/$(MYFILEROOT).ixx;\
	ixIxx $$tmpFile /gbdb/$(DBS)/$(MYFILEROOT).ix /gbdb/$(DBS)/$(MYFILEROOT).ixx;\
	rm -f $$tmpFile	
