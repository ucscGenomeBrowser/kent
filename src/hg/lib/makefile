.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -I../inc -I../../inc -I$(MYSQLINC) -I$(EXPATINC) -c $*.c

L = 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = agpFrag.o agpGap.o bed.o browserTable.o chromInfo.o chromInserts.o \
    clonePos.o cpgIsland.o ctgPos.o customTrack.o cytoBand.o \
    est3.o estPair.o exoFish.o exprBed.o gcPercent.o geneGraph.o genePred.o genomicDups.o \
    glDbRep.o ggCluster.o ggDbIo.o ggDbRep.o ggDump.o \
    ggGraph.o hCommon.o hdb.o hgConfig.o hgFind.o hgGene.o hgRelate.o isochores.o \
    jksql.o knownInfo.o knownMore.o mapSts.o mouseSyn.o \
    ooUtils.o pepPred.o qaSeq.o refLink.o rmskOut.o \
    rnaGene.o roughAli.o simpleRepeat.o snp.o softberryHom.o \
    softPromoter.o stsAlias.o stsMarker.o traceInfo.o trackTable.o wabAli.o \
    xap.o 

../../lib/$(MACHTYPE)/jkhgap.a: $(O) 
	ar rcu ../../lib/$(MACHTYPE)/jkhgap.a $(O)
	ranlib ../../lib/$(MACHTYPE)/jkhgap.a


clean:
	rm -f $(O);
	rm -f ../../lib/$(MACHTYPE)/jkhgap.a;

tags:
	etags *.c *.h ../inc/*.h ../lib/*.c ../../inc/*.h ../../lib/*.c
