include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = vgLoadAllen.o

vgLoadAllen: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/vgLoadAllen $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/vgLoadAllen${EXE}

test:
	vgLoadAllen /san/sanvol1/visiGene/gbdb/full/inSitu/Mouse/allenBrain /san/sanvol1/visiGene/offline/allenBrain/probesAndData/allen20051021.tab /cluster/data/mm7/bed/allenBrain/allProbes.fa /cluster/data/mm7/bed/allenBrain/allProbes.tab output

clean:
	rm -f $O
