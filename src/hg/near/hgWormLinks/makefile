include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgWormLinks.o

hgWormLinks: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgWormLinks $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgWormLinks${EXE}

test:
	cd /cluster/data/ce1/bed/steinHelp && hgWormLinks spWp.ace seqPepDesc.ace wbConcise.txt wbGeneClass.txt ../orfToGene/orfToGene sangerLinks.txt

clean::
	rm -f ${O}
