.c.o:
	gcc -ggdb -O -Wimplicit -I../inc -I../../inc -I$(MYSQLINC) -c $*.c

L = 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = agpFrag.o agpGap.o bed.o chromInfo.o chromInserts.o \
    clonePos.o cpgIsland.o ctgPos.o cytoBand.o \
    est3.o exoFish.o gcPercent.o geneGraph.o genePred.o genomicDups.o \
    glDbRep.o ggCluster.o ggDbIo.o ggDbRep.o ggDump.o \
    ggGraph.o hCommon.o hdb.o hgGene.o hgRelate.o isochores.o \
    jksql.o knownInfo.o knownMore.o mapSts.o mouseSyn.o \
    ooUtils.o pepPred.o psl.o qaSeq.o rmskOut.o \
    rnaGene.o simpleRepeat.o snp.o sqlList.o sqlNum.o \
    stsAlias.o stsMarker.o wabAli.o

../../lib/$(MACHTYPE)/jkhgap.a: $(O) 
	ar rcu ../../lib/$(MACHTYPE)/jkhgap.a $(O)
	ranlib ../../lib/$(MACHTYPE)/jkhgap.a

