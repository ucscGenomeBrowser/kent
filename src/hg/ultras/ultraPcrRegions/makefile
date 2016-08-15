kentSrc = ../../..
include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = ultraPcrRegions.o

ultraPcrRegions: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/ultraPcrRegions $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/ultraPcrRegions${EXE}

clean:
	rm -f $O
