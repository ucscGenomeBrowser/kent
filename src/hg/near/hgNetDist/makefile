include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = hgNetDist
O = $A.o

hgNetDist: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/$A $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/$A${EXE}
	
test1:	
	hgNetDist yeastP2P.tab dm1 sgdP2P -threshold=3
	
test2:	
	hgNetDist flytest.tab dm1 flyP2P -threshold=2 -sqlRemap="select flyBaseId, bdgpName from bdgpGeneFb2Bdgp"
