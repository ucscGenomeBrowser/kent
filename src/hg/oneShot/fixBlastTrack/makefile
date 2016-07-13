kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = fixBlastTrack.o

fixBlastTrack: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/fixBlastTrack $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/fixBlastTrack${EXE}

clean:
	rm -f $O
