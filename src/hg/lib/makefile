XINC = -I$(MYSQLINC)
include ../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 

O = affyAtlas.o agpFrag.o agpGap.o altGraph.o altGraphX.o ancientRref.o \
    axtInfo.o axtLib.o bbRepMask.o bdgpGeneInfo.o bed.o blastTab.o \
    blastzNet.o blatServers.o \
    borf.o borkPseudoHom.o cart.o cartDb.o \
    celeraDupPositive.o celeraCoverage.o cgh.o \
    chainDb.o chainLink.o chainNet.o chainNetDbLoad.o \
    chromInfo.o chromInserts.o chr18deletions.o \
    clonePos.o coordConv.o cpgIsland.o ctgPos.o customTrack.o cytoBand.o \
    defaultDb.o dbDb.o dnaMotif.o dnaProbe.o ensFace.o \
    est3.o estOrientInfo.o estPair.o exoFish.o expData.o expRecord.o \
    featureBits.o fishClones.o flyBaseSwissProt.o gcPercent.o geneBands.o geneGraph.o \
    genePred.o genomicDups.o genomicSuperDups.o \
    ggCluster.o ggDbIo.o ggGraph.o goa.o goaPart.o grp.o \
    hCommon.o hdb.o hgConfig.o hgFind.o \
    hgGene.o hgSeq.o hgRelate.o hui.o humanParalog.o isochores.o \
    jaxOrtholog.o jaxQTL.o jksql.o \
    knownInfo.o knownMore.o lfs.o \
    minGeneInfo.o tigrCmrGene.o llaInfo.o \
    mapSts.o mcnBreakpoints.o mouseSyn.o mouseSynWhd.o mouseOrtho.o \
    ensPhusionBlast.o netAlign.o pseudoGeneLink.o \
    ooUtils.o pepPred.o plasEndPairs.o pslWScore.o \
    qaSeq.o recombRate.o kgAlias.o kgProtAlias.o findKGAlias.o findKGProtAlias.o \
    refLink.o riken.o rmskOut.o \
    rnaGene.o rnaGroup.o rikenBest.o rikenCluster.o roughAli.o \
    sage.o sageCounts.o sageExp.o sanger22extra.o \
    scoredRef.o simpleRepeat.o snp.o softberryHom.o \
    softPromoter.o spDb.o stsAlias.o stsInfo.o stsInfo2.o stsInfoRat.o\
    stsMap.o stsMapMouse.o  stsMarker.o stsInfoMouse.o synteny100000.o\
    stsMapMouseNew.o stsInfoMouseNew.o\
    syntenyBerk.o syntenySanger.o tilingPath.o traceInfo.o \
    trackDb.o trackDbCustom.o trackTable.o wabAli.o \
    ggMrnaAli.o genbank.o \
    glDbRep.o ggDump.o sample.o exprBed.o genMapDb.o \
    dbSnpRS.o synMap.o refSeqStatus.o web.o affyPairs.o affyOffset.o columnInfo.o \
    imageClone.o stanMad.o bactigPos.o stsMapRat.o vegaInfo.o gbProtAnn.o \
    wiggle.o

../../lib/$(MACHTYPE)/jkhgap.a: $(O) 
	ar rcus ../../lib/$(MACHTYPE)/jkhgap.a $(O)

clean:
	rm -f $(O);
	rm -f ../../lib/$(MACHTYPE)/jkhgap.a;

ctags:
	ctags *.c *.h ../inc/*.h ../lib/*.c ../../inc/*.h ../../lib/*.c

tags:
	etags *.c *.h ../inc/*.h ../lib/*.c ../../inc/*.h ../../lib/*.c

test:
	cd tests && runtest

tests: 	
#	gcc -Wimplicit -ggdb   -I../../inc -I../inc  -o test_ggMrnaAli test_ggMrnaAli.c  $(MYLIBS) $L
	gcc -Wimplicit -ggdb   -I../../inc -I../inc  -o test_geneGraph test_geneGraph.c  $(MYLIBS) $L
