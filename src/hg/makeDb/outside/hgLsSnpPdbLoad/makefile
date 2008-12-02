ROOT=../../../..
include ${ROOT}/inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ${ROOT}/lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgLsSnpPdbLoad.o

hgLsSnpPdbLoad: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgLsSnpPdbLoad $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgLsSnpPdbLoad${EXE}

clean:
	rm -f $O
