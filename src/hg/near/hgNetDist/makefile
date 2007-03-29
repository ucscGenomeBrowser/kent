include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = hgNetDist
O = $A.o

hgNetDist: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/$A $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/$A${EXE}

hprdP2p: hprdP2p.o $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hprdP2p hprdP2p.o $(MYLIBS) $L
	${STRIP} ${BINDIR}/hprdP2p${EXE}

hprdP2pTest: ${BINDIR}/hprdP2p${EXE}
	hprdP2p -verbose=2 \
		/cluster/data/hg18/p2p/hprd/FLAT_FILES/BINARY_PROTEIN_PROTEIN_INTERACTIONS.txt \
		/cluster/data/hg18/p2p/hprd/FLAT_FILES/PROTEIN_COMPLEXES.txt \
		hprd.p2p			
	
yeast:	
	hgNetDist yeastP2P.tab sacCer1 yeastP2P -threshold=3
	
fly:	
	hgNetDist flytest.tab dm1 flyP2P -threshold=2 -sqlRemap="select flyBaseId, bdgpName from bdgpGeneFb2Bdgp"
	
worm:	
	hgNetDist wormP2P.tab ce2 wormP2P -threshold=2
	
clean:
	rm -f $O hprdP2p.o
