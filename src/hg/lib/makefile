OPT= -ggdb 

#Optimized for performance
.c.o:
	gcc -ggdb -O -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -Wformat -Wimplicit -Wuninitialized -I../inc -I../../inc -I$(MYSQLINC) -c $*.c

#For debugging
#.c.o:
#	gcc -ggdb -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -Wformat -Wimplicit  -I../inc -I../../inc -I$(MYSQLINC) -c $*.c

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 

O = agpFrag.o agpGap.o axtInfo.o bbRepMask.o bed.o blatServers.o blastzNet.o \
    borf.o borkPseudoHom.o cart.o cartDb.o cgh.o \
    chainDb.o chainLink.o chainNet.o chainNetDbLoad.o \
    chromInfo.o chromInserts.o chr18deletions.o \
    clonePos.o cpgIsland.o ctgPos.o customTrack.o cytoBand.o \
    dbDb.o dnaMotif.o est3.o estOrientInfo.o estPair.o exoFish.o \
    featureBits.o fishClones.o gcPercent.o geneBands.o geneGraph.o \
    genePred.o genomicDups.o \
    genomicSuperDups.o celeraDupPositive.o celeraCoverage.o\
    ggCluster.o altGraph.o ggDbIo.o \
    ggGraph.o grp.o hCommon.o hdb.o hgConfig.o hgFind.o \
    hgGene.o hgRelate.o hui.o humanParalog.o isochores.o jaxOrtholog.o jaxQTL.o\
    jksql.o knownInfo.o knownMore.o lfs.o stsMap.o stsMapMouse.o \
    jksql.o knownInfo.o knownMore.o lfs.o \
    mapSts.o mcnBreakpoints.o mouseSyn.o mouseSynWhd.o mouseOrtho.o \
    ensPhusionBlast.o netAlign.o \
    ooUtils.o pepPred.o plasEndPairs.o pslWScore.o \
    qaSeq.o recombRate.o refLink.o riken.o rmskOut.o \
    rnaGene.o rnaGroup.o rikenBest.o rikenCluster.o roughAli.o \
    sage.o sageExp.o sanger22extra.o \
    simpleRepeat.o snp.o softberryHom.o \
    softPromoter.o stsAlias.o stsInfo.o stsInfo2.o \
    stsMap.o stsMapMouse.o stsMarker.o stsInfoMouse.o synteny100000.o\
    syntenyBerk.o syntenySanger.o tilingPath.o traceInfo.o trackDb.o \
    trackDbCustom.o trackDb.o trackTable.o wabAli.o \
    sage.o sageExp.o pslWScore.o coordConv.o expRecord.o dnaProbe.o \
    ancientRref.o ggMrnaAli.o \
    glDbRep.o ggDump.o altGraphX.o sample.o exprBed.o genMapDb.o \
    dbSnpRS.o synMap.o refSeqStatus.o web.o affyPairs.o affyOffset.o columnInfo.o \
    affyAtlas.o imageClone.o stanMad.o jaxQTL.o hgSeq.o bactigPos.o 

../../lib/$(MACHTYPE)/jkhgap.a: $(O) 
	ar rcus ../../lib/$(MACHTYPE)/jkhgap.a $(O)

clean:
	rm -f $(O);
	rm -f ../../lib/$(MACHTYPE)/jkhgap.a;

ctags:
	ctags *.c *.h ../inc/*.h ../lib/*.c ../../inc/*.h ../../lib/*.c

tags:
	etags *.c *.h ../inc/*.h ../lib/*.c ../../inc/*.h ../../lib/*.c

tests: 	
#	gcc -Wimplicit -ggdb   -I../../inc -I../inc  -o test_ggMrnaAli test_ggMrnaAli.c  $(MYLIBS) $L
	gcc -Wimplicit -ggdb   -I../../inc -I../inc  -o test_geneGraph test_geneGraph.c  $(MYLIBS) $L
