kentSrc = ../../..
include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

GOLDGAP_O = hgGoldGapGl.o ntContig.o
LOADGAP_O = hgLoadGap.o

all: hgGoldGapGl hgLoadGap

hgGoldGapGl: ${GOLDGAP_O} ../../../lib/${MACHTYPE}/jkweb.a
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/hgGoldGapGl \
		${GOLDGAP_O} ${MYLIBS} $L

hgLoadGap: ${LOADGAP_O}  ../../../lib/${MACHTYPE}/jkweb.a
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/hgLoadGap \
		${LOADGAP_O} ${MYLIBS} $L

lib:
	cd ../../../lib && ${MAKE}

compile: ${GOLDGAP_O} ${LOADGAP_O}  ../../../lib/${MACHTYPE}/jkweb.a
	${CC} ${COPT} -o hgGoldGapGl ${GOLDGAP_O} ${MYLIBS} $L
	${CC} ${COPT} -o hgLoadGap ${LOADGAP_O} ${MYLIBS} $L

clean::
	rm -f hgGoldGapGl hgLoadGap ${GOLDGAP_O} ${LOADGAP_O}
