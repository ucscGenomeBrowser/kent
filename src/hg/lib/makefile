XINC = -I$(MYSQLINC)
include ../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 

O = affyAtlas.o affyGenoDetails.o affyOffset.o affyPairs.o agpFrag.o \
    agpGap.o altGraph.o altGraphX.o ancientRref.o axtInfo.o \
    axtLib.o bactigPos.o bbRepMask.o bdgpGeneInfo.o bed.o \
    blastTab.o blastzNet.o blatServers.o borf.o borkPseudoHom.o \
    cart.o cartDb.o celeraCoverage.o celeraDupPositive.o cgh.o \
    chainDb.o chainLink.o chainNet.o chainNetDbLoad.o chr18deletions.o \
    chromInfo.o chromInserts.o clonePos.o columnInfo.o coordConv.o \
    cpgIsland.o ctgPos.o customTrack.o cytoBand.o dbDb.o \
    dbSnpRS.o defaultDb.o dnaMotif.o dnaProbe.o encodeRegionInfo.o \
    encodeRegionInfoCustom.o ensFace.o ensPhusionBlast.o est3.o estOrientInfo.o \
    estPair.o exoFish.o expData.o expRecord.o exprBed.o \
    fbTables.o featureBits.o findKGAlias.o findKGProtAlias.o fishClones.o \
    flyBaseSwissProt.o gbProtAnn.o gcPercent.o genMapDb.o genbank.o \
    geneBands.o geneGraph.o genePred.o genomicDups.o genomicSuperDups.o \
    ggCluster.o ggDbIo.o ggDump.o ggGraph.o ggMrnaAli.o \
    glDbRep.o goa.o goaPart.o grp.o hCommon.o \
    hdb.o hgConfig.o hgExp.o hgFind.o hgGene.o \
    hgMaf.o hgRelate.o hgSeq.o hui.o humanParalog.o \
    imageClone.o isochores.o jaxOrtholog.o jaxQTL.o jksql.o \
    kgAlias.o kgProtAlias.o knownInfo.o knownMore.o knownToSuper.o \
    lfs.o llaInfo.o mapSts.o mcnBreakpoints.o minGeneInfo.o \
    mouseOrtho.o mouseSyn.o mouseSynWhd.o netAlign.o ooUtils.o \
    pepPred.o plasEndPairs.o pseudoGeneLink.o pslWScore.o qaSeq.o \
    recombRate.o refLink.o refSeqStatus.o riken.o rikenBest.o \
    rikenCluster.o rmskOut.o rnaFold.o rnaGene.o rnaGroup.o \
    roughAli.o sage.o sageCounts.o sageExp.o sample.o \
    sanger22extra.o scopDes.o scoredRef.o simpleRepeat.o snp.o \
    softPromoter.o softberryHom.o spDb.o stanMad.o stsAlias.o \
    stsInfo.o stsInfo2.o stsInfoMouse.o stsInfoMouseNew.o stsInfoRat.o \
    stsMap.o stsMapMouse.o stsMapMouseNew.o stsMapRat.o stsMarker.o \
    synMap.o synteny100000.o syntenyBerk.o syntenySanger.o tableDescriptions.o \
    tigrCmrGene.o tilingPath.o traceInfo.o trackDb.o trackDbCustom.o \
    trackTable.o vegaInfo.o wabAli.o web.o wiggle.o 

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
