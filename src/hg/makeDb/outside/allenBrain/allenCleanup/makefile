include ../../../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = allenCleanup.o

allenCleanUp: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/allenCleanup $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/allenCleanup${EXE}

test: allenCleanUp
	allenCleanup \
	/san/sanvol1/visiGene/offline/allenBrain/imageDisk \
	/san/sanvol1/visiGene/gbdb/full/inSitu/Mouse/allenBrain \
     	/san/sanvol1/visiGene/offline/allenBrain/imageDisk/May_06/allen20061204combined.tab \
	/san/sanvol1/visiGene/offline/allenBrain/probesAndData/allen20061204.tab \
	> log


clean:
	rm -f $O
