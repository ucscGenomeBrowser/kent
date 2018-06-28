kentSrc = ../..
include ../../inc/common.mk

#L += ${HALLIBS} ${MYSQLLIBS} -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a ../../htslib/libhts.a ${MYSQLLIBS}

A = hgTrackUi

O = $A.o cgapSageUi.o encodePeakUi.o

include ../../inc/cgi_build_rules.mk

compile: $O 
	${CC} $O ${MYLIBS} #${L}
	mv ${AOUT} $A${EXE}
