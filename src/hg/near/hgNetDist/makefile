include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = hgNetDist
O = $A.o

hgNetDist: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/$A $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/$A${EXE}

hgLoadNetDist: hgLoadNetDist.o $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgLoadNetDist hgLoadNetDist.o $(MYLIBS) $L
	${STRIP} ${BINDIR}/hgLoadNetDist${EXE}

hprdP2p: hprdP2p.o $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hprdP2p hprdP2p.o $(MYLIBS) $L
	${STRIP} ${BINDIR}/hprdP2p${EXE}

hprdP2pTest: ${BINDIR}/hprdP2p${EXE}
	hprdP2p -verbose=2 \
		/cluster/data/hg18/p2p/hprd/FLAT_FILES/BINARY_PROTEIN_PROTEIN_INTERACTIONS.txt \
		/cluster/data/hg18/p2p/hprd/FLAT_FILES/PROTEIN_COMPLEXES.txt \
		/cluster/data/hg18/p2p/hprd/hprd.p2p			
	
yeast:	
	hgNetDist -verbose=2 -skipFirst -threshold=3 \
		/cluster/data/sacCer1/p2p/yeastP2P.tab \
		/cluster/data/sacCer1/p2p/yeastP2P.pathLengths
	hgLoadNetDist /cluster/data/sacCer1/p2p/yeastP2P.pathLengths sacCer1 yeastP2P
	
fly:
	hgNetDist -verbose=2 -skipFirst -threshold=2 \
		/cluster/data/dm2/p2p/flyP2P.tab \
		/cluster/data/dm2/p2p/flyP2P.pathLengths 
	hgLoadNetDist /cluster/data/dm2/p2p/flyP2P.pathLengths dm2 flyP2P \
-sqlRemap="select fbgn,name from flyBase2004Xref"
	
	
worm:	
	hgNetDist -skipFirst -threshold=2 \
		/cluster/data/ce2/p2p/wormP2P.tab \
		/cluster/data/ce2/p2p/wormP2P.pathLengths
	hgLoadNetDist /cluster/data/ce2/p2p/wormP2P.pathLengths ce2 wormP2P
	
clean:
	rm -f $O hprdP2p.o
