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
	bdfToGem -verbose=4 -name="Tiny" 75dpi/helvR08-L1.bdf mgTinyFont.c
	bdfToGem -noHeader -verbose=4 -name="TinyBold" 75dpi/helvB08-L1.bdf stdout >> mgTinyFont.c
	bdfToGem -noHeader -verbose=4 -name="TinyFixed" misc/5x8-L1.bdf stdout >> mgTinyFont.c

mgSmallFont.c:	${O}
	bdfToGem -verbose=2 -name="Small" 75dpi/helvR10-L1.bdf mgSmallFont.c
	bdfToGem -noHeader -verbose=2 -name="SmallBold" 75dpi/helvB10-L1.bdf stdout >> mgSmallFont.c
	bdfToGem -noHeader -verbose=2 -name="SmallFixed" misc/6x10-L1.bdf stdout >> mgSmallFont.c

mgMediumFont.c:	${O}
	bdfToGem -verbose=2 -name="Medium" 75dpi/helvR14-L1.bdf mgMediumFont.c
	bdfToGem -noHeader -verbose=2 -name="MediumBold" 75dpi/helvB14-L1.bdf stdout >> mgMediumFont.c
	bdfToGem -noHeader -verbose=2 -name="MediumFixed" misc/8x13-L1.bdf stdout >> mgMediumFont.c

mgLargeFont.c:	${O}
	bdfToGem -verbose=2 -name="Large" 75dpi/helvR18-L1.bdf mgLargeFont.c
	bdfToGem -noHeader -verbose=2 -name="LargeBold" 75dpi/helvB18-L1.bdf stdout >> mgLargeFont.c
	bdfToGem -noHeader -verbose=2 -name="LargeFixed" misc/10x20-L1.bdf stdout >> mgLargeFont.c

mgHugeFont.c:	${O}
	bdfToGem -verbose=2 -name="Huge" 100dpi/helvR24-L1.bdf mgHugeFont.c
	bdfToGem -noHeader -verbose=2 -name="HugeBold" 100dpi/helvB24-L1.bdf stdout >> mgHugeFont.c
	bdfToGem -noHeader -verbose=2 -name="HugeFixed" misc/12x24.bdf stdout >> mgHugeFont.c

clean:
	rm -f ${HOME}/bin/${MACHTYPE}/${A} $O \
		mgTinyFont.c mgSmallFont.c mgMediumFont.c mgLargeFont.c \
		mgHugeFont.c
