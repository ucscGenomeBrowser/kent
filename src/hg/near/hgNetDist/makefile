kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = hgNetDist
O = $A.o

all: hgNetDist hgLoadNetDist hprdP2p

hgNetDist: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/$A $O $(MYLIBS) $L
	${STRIP} ${DESTDIR}${BINDIR}/$A${EXE}

hgLoadNetDist: hgLoadNetDist.o $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/hgLoadNetDist hgLoadNetDist.o $(MYLIBS) $L
	${STRIP} ${DESTDIR}${BINDIR}/hgLoadNetDist${EXE}

hprdP2p: hprdP2p.o $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/hprdP2p hprdP2p.o $(MYLIBS) $L
	${STRIP} ${DESTDIR}${BINDIR}/hprdP2p${EXE}

hprdP2pHg18: ${DESTDIR}${BINDIR}/hprdP2p${EXE}
	hprdP2p -verbose=2 \
		/cluster/data/hg18/p2p/hprd/FLAT_FILES/BINARY_PROTEIN_PROTEIN_INTERACTIONS.txt \
		/cluster/data/hg18/p2p/hprd/FLAT_FILES/PROTEIN_COMPLEXES.txt \
		/cluster/data/hg18/p2p/hprd/hprd.p2p			

hprdHg18:
	hgNetDist -verbose=2 -weighted -threshold=2 \
		/cluster/data/hg18/p2p/hprd/hprd.p2p \
		/cluster/data/hg18/p2p/hprd/hprd.pathLengths
	hgLoadNetDist /cluster/data/hg18/p2p/hprd/hprd.pathLengths hg18 humanHprdP2P \
		-sqlRemap="select distinct value, name from knownToHprd"

vidalHg18:
	hgNetDist -verbose=2 -skipFirst -threshold=2 \
		/cluster/data/hg18/p2p/vidal/humanVidal.p2p \
		/cluster/data/hg18/p2p/vidal/humanVidal.pathLengths
	hgLoadNetDist /cluster/data/hg18/p2p/vidal/humanVidal.pathLengths hg18 humanVidalP2P \
	-sqlRemap="select distinct locusLinkID, kgID from refLink, kgXref where refLink.mrnaAcc = kgXref.mRNA"

wankerHg18:
	hgNetDist -verbose=2 -skipFirst -threshold=2 \
		/cluster/data/hg18/p2p/wanker/humanWanker.p2p \
		/cluster/data/hg18/p2p/wanker/humanWanker.pathLengths
	hgLoadNetDist /cluster/data/hg18/p2p/wanker/humanWanker.pathLengths hg18 humanWankerP2P \
	-sqlRemap="select distinct locusLinkID, kgID from refLink, kgXref where refLink.mrnaAcc = kgXref.mRNA"


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
	
clean::
	rm -f $O hprdP2p.o
