include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = hgNetDist
O = $A.o

hgNetDist: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/$A $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/$A${EXE}
	
yeast:	
	hgNetDist yeastP2P.tab sacCer1 yeastP2P -threshold=3
	
fly:	
	hgNetDist flytest.tab dm1 flyP2P -threshold=2 -sqlRemap="select flyBaseId, bdgpName from bdgpGeneFb2Bdgp"
	
worm:	
	hgNetDist wormP2P.tab ce2 wormP2P -threshold=2
	
