OPT= -ggdb 

.c.o:
	gcc $(OPT) -Wformat -Wimplicit -Wuninitialized -I../inc -I../../inc -I$(MYSQLINC) -c $*.c

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 

O = agpFrag.o agpGap.o bbRepMask.o bed.o blatServers.o \
    cart.o cartDb.o chromInfo.o chromInserts.o cgh.o \
    clonePos.o cpgIsland.o ctgPos.o customTrack.o cytoBand.o \
    dbDb.o est3.o estPair.o exoFish.o exprBed.o \
    featureBits.o fishClones.o gcPercent.o geneGraph.o \
    genePred.o genomicDups.o \
    ggCluster.o altGraph.o ggDbIo.o \
    ggGraph.o hCommon.o hdb.o hgConfig.o hgFind.o \
    hgGene.o hgRelate.o hui.o isochores.o jaxOrtholog.o \
    jksql.o knownInfo.o knownMore.o lfs.o stsMap.o \
    mapSts.o mcnBreakpoints.o mouseSyn.o \
    ooUtils.o pepPred.o pslWScore.o qaSeq.o refLink.o rmskOut.o \
    rnaGene.o roughAli.o sage.o sageExp.o sanger22extra.o \
    simpleRepeat.o snp.o softberryHom.o \
    softPromoter.o stsAlias.o stsInfo.o stsMarker.o \
    tilingPath.o traceInfo.o trackDb.o \
    trackDbCustom.o trackDb.o trackTable.o wabAli.o refAlign.o \
    sage.o sageExp.o pslWScore.o coordConv.o expRecord.o dnaProbe.o \
    ancientRref.o ggMrnaAli.o \
    glDbRep.o ggDump.o

../../lib/$(MACHTYPE)/jkhgap.a: $(O) 
	ar rcu ../../lib/$(MACHTYPE)/jkhgap.a $(O)
	ranlib ../../lib/$(MACHTYPE)/jkhgap.a

clean:
	rm -f $(O);
	rm -f ../../lib/$(MACHTYPE)/jkhgap.a;

tags:
	etags *.c *.h ../inc/*.h ../lib/*.c ../../inc/*.h ../../lib/*.c

tests: 	
#	gcc -Wimplicit -ggdb   -I../../inc -I../inc  -o test_ggMrnaAli test_ggMrnaAli.c  $(MYLIBS) $L
	gcc -Wimplicit -ggdb   -I../../inc -I../inc  -o test_geneGraph test_geneGraph.c  $(MYLIBS) $L