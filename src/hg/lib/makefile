kentSrc = ../..
include ../../inc/localEnvironment.mk
include ../../inc/common.mk
XINC = -I$(MYSQLINC)

O=adjacency.o affyPairs.o agpFrag.o agpGap.o alignSeqSizes.o altGraphX.o \
  asmAlias.o asmEquivalent.o asmSummary.o assemblyList.o autoUpgrade.o axtInfo.o \
  barChartBed.o barChartCategory.o barChartUi.o bed.o bed12Source.o \
  bedDetail.o bedMethyl.o bigBedFind.o bigBedLabel.o bigChain.o bigDbSnp.o bigGenePred.o bigLink.o \
  bigPsl.o blastTab.o borf.o botDelay.o cart.o cartDb.o cartTrackDb.o cdsEvidence.o cdsOrtho.o cdsPick.o \
  cgapSage/cgapSage.o cgapSage/cgapSageLib.o cgiApoptosis.o chainCart.o chainDb.o chainLink.o chainNet.o \
  chainNetDbLoad.o chromAlias.o chromBins.o chromGraph.o chromGraphFactory.o chromInfo.o chromInserts.o \
  chromKeeper.o clonePos.o cpgIsland.o ctgPos.o customAdjacency.o customComposite.o customFactory.o \
  customPp.o customTrack.o cv.o cytoBand.o dbDb.o decoration.o decoratorUi.o defaultDb.o dnaMotifSql.o dupTrack.o \
  encode/encodeErge.o encode/encodeErgeHssCellLines.o encode/encodeExp.o encode/encodeHapMapAlleleFreq.o \
  encode/encodeIndels.o encode/encodePeak.o encode/encodeRegionInfoCustom.o encode/encodeRna.o \
  encode/encodeStanfordPromoters.o encode/encodeStanfordPromotersAverage.o encode/peptideMapping.o \
  encode/wgEncodeGencodeAnnotationRemark.o encode/wgEncodeGencodeAttrs.o encode/wgEncodeGencodeEntrezGene.o \
  encode/wgEncodeGencodeExonSupport.o encode/wgEncodeGencodeGeneSource.o  encode/wgEncodeGencodeGeneSymbol.o encode/wgEncodeGencodePdb.o \
  encode/wgEncodeGencodePubMed.o encode/wgEncodeGencodeRefSeq.o encode/wgEncodeGencodeTag.o \
  encode/wgEncodeGencodeTranscriptSource.o encode/wgEncodeGencodeTranscriptSupport.o \
  encode/wgEncodeGencodeTranscriptionSupportLevel.o encode/wgEncodeGencodeUniProt.o encode3/encode3Valid.o \
  estOrientInfo.o expData.o exportedDataHubs.o expRecord.o facetField.o facetedTable.o featureBits.o findKGAlias.o \
  fakeCurl.o \
  findKGProtAlias.o gbSeq.o gbExtFile.o gcPercent.o genark.o genbank.o genbankBlackList.o gencodeTracksCommon.o gencodeAttrs.o gencodeToRefSeq.o geneGraph.o \
  genePred.o genePredReader.o geoMirror.o ggCluster.o ggDump.o ggGraph.o ggMrnaAli.o ggTypes.o glDbRep.o \
  googleAnalytics.o gpFx.o grp.o gtexAse.o gtexDonor.o gtexGeneBed.o gtexInfo.o gtexSample.o \
  gtexSampleData.o gtexTissue.o gtexTissueMedian.o gtexUi.o hCommon.o hPrint.o hVarSubst.o \
  hapmapAllelesOrtho.o hapmapPhaseIIISummary.o hapmapSnps.o hdb.o hgColors.o hgConfig.o hgFind.o \
  hgFindSpec.o hgFindSpecCustom.o hgHgvs.o hgHgvsParse.o hgMaf.o hgRelate.o hic.o hicUi.o hubConnect.o hubPublic.o \
  hubSearchText.o hubSpace.o hubSpaceKeys.o hubSpaceQuotas.o hui.o imageClone.o indelShift.o interact.o interactUi.o itemAttr.o jksql.o joiner.o \
  jsHelper.o kgAlias.o kgProtAlias.o kgXref.o knownInfo.o knownMore.o lav.o lfs.o liftOver.o liftOverChain.o \
  liftUp.o longRange.o lrg.o lsSnpPdb.o mafFrames.o mafGene.o mafSummary.o makeItemsItem.o mdb.o \
  microarray.o minChromSize.o namedSessionDb.o netAlign.o netCart.o pepPred.o pgPhenoAssoc.o pgSnp.o pslReader.o qaSeq.o \
  quickLift.o quickLiftChain.o rankProp.o refLink.o refSeqStatus.o rikenCluster.o rmskAlign.o rmskJoined.o rmskOut.o rmskOut2.o rnaGene.o sage.o sageCounts.o samAlignment.o \
  sample.o sanger22extra.o scoredRef.o sessionData.o seqWindow.o snakeUi.o snp.o snpExceptions.o soTerm.o spDb.o sqlProg.o \
  sqlSanity.o stanMad.o stsAlias.o stsInfo2.o stsMap.o stsMarker.o tableStatus.o tablesTables.o \
  tagRepo.o tfbsCons.o tigrCmrGene.o traceInfo.o trackDb.o trackDbCache.o trackDbCustom.o trackHub.o \
  trackVersion.o trashDir.o txAliDiff.o txCluster.o txCommon.o txEdgeBed.o txGraph.o txInfo.o txRnaAccs.o \
  userdata.o variantProjector.o vcfUi.o vegaInfo.o web.o wigAsciiToBinary.o wigDataStream.o wiggle.o wiggleCart.o \
  wiggleUtils.o wikiLink.o wikiTrack.o bigRmskAlignBed.o bigRmskBed.o

ifeq (${USE_HIC}, 1)
  O += straw.o
endif

ifeq (${GBROWSE}, 1)
  GBROWSE_D=-DGBROWSE
else
  GBROWSE_D=
endif

%.o: %.c
	${CC} ${COPT} ${CFLAGS} ${GBROWSE_D} ${LOWELAB_DEFS} ${HG_DEFS} ${HG_WARN} ${HG_INC} ${XINC} -o $@ -c $<

../../lib/$(MACHTYPE)/jkhgap.a: $(O) libDir
	ar rcus ../../lib/$(MACHTYPE)/jkhgap.a $(O)

libDir:
	mkdir -p ../../lib/$(MACHTYPE)

jWestHeader.h: jWestHeader.html
	sed -e 's/\\/\\\\/g; s/"/\\"/g; s/^/"/; s/$$/\\n"/;' jWestHeader.html > jWestHeader.h

jWestBanner.h: jWestBanner.html
	sed -e 's/\\/\\\\/g; s/"/\\"/g; s/^/"/; s/$$/\\n"/;' jWestBanner.html > jWestBanner.h

web.o: jWestHeader.h jWestBanner.h

straw.o: straw/straw.cpp straw/straw.h straw/cStraw.cpp
	cd straw && ${MAKE} straw

clean:
	rm -f $(O);
	rm -f jWestHeader.h jWestBanner.h
	rm -f ../../lib/$(MACHTYPE)/jkhgap.a;
	cd straw && ${MAKE} clean

ctags:
	ctags *.c *.h ../inc/*.h ../lib/*.c ../../inc/*.h ../../lib/*.c ../inc/encode/*.h ../lib/encode/*.c

tags:
	etags *.c *.h ../inc/*.h ../lib/*.c ../../inc/*.h ../../lib/*.c ../inc/encode/*.h ../lib/encode/*.c

test:
	cd tests && ${MAKE} test
