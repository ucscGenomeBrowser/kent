kentSrc = ../..
include ../../inc/common.mk

L += -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS = ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a 

XINC=-I../../lib

O = bdfToGem.o

A = bdfToGem

all::	${A} mgTinyFont.c mgSmallFont.c mgMediumFont.c mgLargeFont.c \
		mgHugeFont.c

bold::	${A} tinyBold smallBold mediumBold largeBold hugeBold

fixed::	${A} tinyFixed smallFixed mediumFixed largeFixed hugeFixed

allThree::	all bold fixed

$A: $O ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} $L
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

compile:	${O} ${MYLIBS}
	${CC} ${COPT} -o ${A}${EXE} $O ${MYLIBS} $L

install:: compile
	rm -f ${DESTDIR}${BINDIR}/${A}${EXE}
	cp -p ${A}${EXE} ${DESTDIR}${BINDIR}/${A}${EXE}
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

mgTinyFont.c:	${O}
	bdfToGem -verbose=2 -name="Tiny" 75dpi/helvR08-L1.bdf mgTinyFont.c

mgSmallFont.c:	${O}
	bdfToGem -verbose=2 -name="Small" 75dpi/helvR10-L1.bdf mgSmallFont.c

mgMediumFont.c:	${O}
	bdfToGem -verbose=2 -name="Medium" 75dpi/helvR14-L1.bdf mgMediumFont.c

mgLargeFont.c:	${O}
	bdfToGem -verbose=2 -name="Large" 75dpi/helvR18-L1.bdf mgLargeFont.c

mgHugeFont.c:	${O}
	bdfToGem -verbose=2 -name="Huge" 100dpi/helvR24-L1.bdf mgHugeFont.c

tinyBold:	mgTinyFont.c
	bdfToGem -noHeader -verbose=2 -name="TinyBold" \
		75dpi/helvB08-L1.bdf stdout >> mgTinyFont.c

smallBold:	mgSmallFont.c
	bdfToGem -noHeader -verbose=2 -name="SmallBold" \
		75dpi/helvB10-L1.bdf stdout >> mgSmallFont.c

mediumBold:	mgMediumFont.c
	bdfToGem -noHeader -verbose=2 -name="MediumBold" \
		75dpi/helvB14-L1.bdf stdout >> mgMediumFont.c

largeBold:	mgLargeFont.c
	bdfToGem -noHeader -verbose=2 -name="LargeBold" \
		75dpi/helvB18-L1.bdf stdout >> mgLargeFont.c

hugeBold:	mgHugeFont.c
	bdfToGem -noHeader -verbose=2 -name="HugeBold" \
		100dpi/helvB24-L1.bdf stdout >> mgHugeFont.c


tinyFixed:	mgTinyFont.c
	bdfToGem -noHeader -verbose=2 -name="TinyFixed" \
		misc/5x8-L1.bdf stdout >> mgTinyFont.c

smallFixed:	mgSmallFont.c
	bdfToGem -noHeader -verbose=2 -name="SmallFixed" \
		misc/6x10-L1.bdf stdout >> mgSmallFont.c

mediumFixed:	mgMediumFont.c
	bdfToGem -noHeader -verbose=2 -name="MediumFixed" \
		misc/8x13-L1.bdf stdout >> mgMediumFont.c

largeFixed:	mgLargeFont.c
	bdfToGem -noHeader -verbose=2 -name="LargeFixed" \
		misc/9x15-L1.bdf stdout >> mgLargeFont.c

#	there is a 12x24 misc font, but it is different than
#	the other fixed fonts

hugeFixed:	mgHugeFont.c
	bdfToGem -noHeader -verbose=2 -name="HugeFixed" \
		misc/10x20-L1.bdf stdout >> mgHugeFont.c

clean:
	rm -f ${BINDIR}/${A} $O \
		mgTinyFont.c mgSmallFont.c mgMediumFont.c mgLargeFont.c \
		mgHugeFont.c
