include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = fixBlastTrack.o

fixBlastTrack: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/fixBlastTrack $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/fixBlastTrack

clean:
	rm -f $O
