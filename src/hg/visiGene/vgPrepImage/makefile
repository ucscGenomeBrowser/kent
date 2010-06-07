
include ../../../inc/common.mk

ifeq (${USE_JPG2000},1)
    ERMAPPER = /scratch/galt/ecw-jpeg/libecwj2-3.3
    XINC = -I${ERMAPPER}/Source/NCSBuildQmake -I${ERMAPPER}/Source/include -I/usr/include -I.
    ERLIBS  = -lpthread ${ERMAPPER}/lib/libecwj2.a
else
    XINC = -I/usr/include -I.
    ERLIBS  = -lpthread
endif

CFLAGS = -pipe -Wall -W -O2 -DLINUX -DUNICODE -D_REENTRANT -DPOSIX

L += -lm -ljpeg
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a ${ERLIBS}

# required for some .cpp elements in libecwj2.a
LINK=g++

A = vgPrepImage

O = ${A}.o jp2Dec.o jpgDec.o jpgTiles.o

${A}: $O ${MYLIBS}
	${LINK} ${COPT} -o ${BINDIR}/${A} $O ${MYLIBS} $L
	strip ${BINDIR}/${A}

install: ${BINDIR}/${A}
	cp ${BINDIR}/${A} /cluster/bin/${MACHTYPE}/

example:
	vgPrepImage /san/sanvol1/visiGene/offline/allenBrain/imageDisk \
	/san/sanvol1/visiGene/gbdb/full/inSitu/Mouse/allenBrain \
	/san/sanvol1/visiGene/gbdb/full/inSitu/Mouse/allenBrain \
	prod3/LOC432413_124_0205042191_D.jp2

clean:
	rm -f $O
