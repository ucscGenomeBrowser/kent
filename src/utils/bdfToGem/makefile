include ../../inc/common.mk

L = -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS = ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

XINC=-I../../lib

O = bdfToGem.o

A = bdfToGem

all::	${A} mgTinyFont.c mgSmallFont.c mgMediumFont.c mgLargeFont.c \
		mgHugeFont.c

$A: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/${A}${EXE} $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/${A}${EXE}

compile:	${O} ${MYLIBS}
	${CC} ${COPT} -o ${A}${EXE} $O ${MYLIBS} $L

mgTinyFont.c:	${O}
	bdfToGem -verbose=2 -name="Tiny" 75dpi/helvR08.bdf mgTinyFont.c

mgSmallFont.c:	${O}
	bdfToGem -verbose=4 -name="Small" 75dpi/helvR10.bdf mgSmallFont.c

mgMediumFont.c:	${O}
	bdfToGem -verbose=2 -name="Medium" 75dpi/helvR14.bdf mgMediumFont.c

mgLargeFont.c:	${O}
	bdfToGem -verbose=2 -name="Large" 75dpi/helvR18.bdf mgLargeFont.c

mgHugeFont.c:	${O}
	bdfToGem -verbose=4 -name="Huge" 100dpi/helvR24.bdf mgHugeFont.c

clean:
	rm -f ${HOME}/bin/${MACHTYPE}/${A} $O \
		mgTinyFont.c mgSmallFont.c mgMediumFont.c mgLargeFont.c \
		mgHugeFont.c
