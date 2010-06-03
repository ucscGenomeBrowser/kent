include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = makeTrackIndex

O = $A.o

dir = /cluster/home/larrym/kent/src/hg/utils/makeTrackIndex

include ../../../inc/cgi_build_rules.mk

compile: $O
	${CC} -o $A $O ${MYLIBS} ${L}
