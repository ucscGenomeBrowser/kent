include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = fixBlastTrack.o

fixBlastTrack: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/fixBlastTrack $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/fixBlastTrack${EXE}

clean:
	rm -f $O
