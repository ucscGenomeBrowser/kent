kentSrc = ../..
include ../../inc/localEnvironment.mk
include ../../inc/common.mk
XINC = -I$(MYSQLINC)

O = acemblyClass.o adjacency.o affyAllExonProbe.o affyAtlas.o affy10KDetails.o affy120KDetails.o \
    affyOffset.o affyPairs.o agp.o agpFrag.o agpGap.o alignSeqSizes.o altGraph.o \
    altGraphX.o ancientRref.o \
    annoFormatVep.o annoGratorGpVar.o annoGrateWigDb.o \
    annoStreamDb.o annoStreamDbFactorSource.o annoStreamDbPslPlus.o annoStreamWig.o \
    api.o atomDb.o autoUpgrade.o axtInfo.o axtLib.o bactigPos.o hgBam.o \
    barChartBed.o barChartCategory.o barChartUi.o barChartData.o barChartSample.o \
    baseMaskCommon.o bdgpExprLink.o bdgpGeneInfo.o \
    bed.o bed5FloatScore.o bed5Pval.o bed6FloatScore.o  bed8Attrs.o bed12Source.o \
    bed12wSeq.o bedCart.o bgiGeneInfo.o bgiGeneSnp.o bgiSnp.o bigBedFind.o bigBedLabel.o \
    bigGenePred.o bigPsl.o bigTransMap.o bioImage.o \
    blastTab.o blastzNet.o blatServers.o borf.o borkPseudoHom.o botDelay.o\
    cart.o cartDb.o cartJson.o cartTrackDb.o cdsEvidence.o cdsOrtho.o cdsPick.o cdsSpec.o \
    ccdsInfo.o ccdsNotes.o ccdsGeneMap.o celeraCoverage.o \
    celeraDupPositive.o cgapSage/cgapSage.o cgapSage/cgapSageLib.o cgh.o chainCart.o \
    chainDb.o chainLink.o chainNet.o chainNetDbLoad.o \
    chicken13kInfo.o chromBins.o chromAlias.o chr18deletions.o \
    chromGraph.o chromGraphFactory.o chromInfo.o chromInserts.o chromKeeper.o \
    clonePos.o codeBlast.o codeBlastScore.o cogs.o cogsxra.o columnInfo.o \
    contigAcc.o coordConv.o \
    cnpIafrate.o cnpIafrate2.o cnpLocke.o cnpRedon.o cnpSebat.o cnpSebat2.o \
    cnpSharp2.o cnpSharpCutoff.o cnpSharpSample.o cnpSharp.o \
    cpgIsland.o cpgIslandExt.o ctgPos.o ctgPos2.o customComposite.o \
    bedDetail.o cgiApoptosis.o customAdjacency.o \
    customFactory.o customPp.o customTrack.o cutter.o cv.o cytoBand.o \
    dbDb.o dbRIP.o dbSnpRs.o defaultDb.o delConrad2.o delHinds2.o \
    dgv.o dgvPlus.o dless.o dnaMotifSql.o dnaMarkovSql.o dnaProbe.o \
    dv.o dvBed.o dvXref2.o \
    easyGene.o ec.o ecCode.o ecAttribute.o ecAttributeCode.o \
    encode/encodeErge.o encode/encodeErgeHssCellLines.o \
    encode/encodeHapMapAlleleFreq.o encode/encodeIndels.o encode/encodePeak.o \
    encode/encodeRegionInfo.o encode/encodeRegionInfoCustom.o encode/encodeRna.o \
    encode/encodeStanfordPromoters.o encode/encodeStanfordPromotersAverage.o \
    encode/pairedTagAlign.o encode/peptideMapping.o encode/tagAlign.o  \
    encode/wgEncodeGencodeAttrs.o encode/wgEncodeGencodeExonSupport.o \
    encode/wgEncodeGencodeEntrezGene.o encode/wgEncodeGencodeTag.o  \
    encode/wgEncodeGencodeGeneSymbol.o encode/wgEncodeGencodePdb.o encode/wgEncodeGencodePubMed.o \
    encode/wgEncodeGencodeRefSeq.o encode/wgEncodeGencodeGeneSource.o encode/wgEncodeGencodeTranscriptSource.o \
    encode/wgEncodeGencodeTranscriptSupport.o encode/wgEncodeGencodeTranscriptionSupportLevel.o \
    encode/wgEncodeGencodeUniProt.o \
    encode/wgEncodeGencodePolyAFeature.o encode/wgEncodeGencodeAnnotationRemark.o \
    encode/wgEncodeCell.o \
    encode/encodeExp.o \
    encode3/encode2Manifest.o encode3/encode3Valid.o \
    ensFace.o ensInfo.o ensPhusionBlast.o ensXRefZfish.o \
    est3.o estOrientInfo.o \
    estPair.o exoFish.o expData.o expRecord.o exprBed.o facetField.o factorSource.o \
    fbTables.o featureBits.o fileUi.o findKGAlias.o findKGProtAlias.o fishClones.o \
    flyBase2004Xref.o \
    flyBaseSwissProt.o flyreg.o flyreg2.o gbExtFile.o gbWarn.o gbMiscDiff.o gbProtAnn.o gcPercent.o gbSeq.o \
    genbank.o genbankBlackList.o gencodeGeneClass.o gencodeIntron.o genMapDb.o \
    geneBands.o geneCheck.o geneCheckDetails.o geneCheckWidget.o \
    geneGraph.o genePred.o genePredReader.o geneSimilarities.o genoLay.o \
    genomeRangeTreeFile.o genomicDups.o \
    genomicSuperDups.o geoMirror.o ggCluster.o ggDbIo.o ggDump.o ggGraph.o ggMrnaAli.o \
    ggTypes.o glDbRep.o goa.o goaPart.o googleAnalytics.o gpFx.o \
    growthCondition.o grp.o \
    gtexAse.o gtexBoxplot.o gtexEqtlCluster.o gtexInfo.o gtexDonor.o gtexGeneBed.o gtexSample.o \
    gtexSampleData.o gtexTissue.o gtexTissueData.o gtexTissueMedian.o gtexUi.o \
    gwasCatalog.o hAnno.o haplotypes.o \
    hapmapAllelesOrtho.o hapmapAllelesSummary.o hapmapPhaseIIISummary.o \
    hapmapSnps.o hapmapSnpsCombined.o \
    hashJoin.o hCommon.o \
    hCytoBand.o hdb.o hgColors.o hgConfig.o hgExp.o hgFind.o \
    hgFindSpec.o hgFindSpecCustom.o hgHgvs.o hgHgvsParse.o \
    hgGene.o hic.o hicUi.o hgMaf.o hgRelate.o hgSeq.o hgdpGeo.o hPrint.o hVarSubst.o hvGfx.o \
    HInv.o hubConnect.o hubSearchText.o hui.o humanParalog.o hubPublic.o \
    imageClone.o indelShift.o interact.o interactUi.o isochores.o ispyTables.o \
    itemAttr.o itemConf.o itemDetailsHtml.o jalview.o \
    jaxOrtholog.o jaxQTL.o jaxQTL3.o jksql.o joiner.o jsHelper.o kg1ToKg2.o \
    jgiGene.o joinMixer.o \
    kgAlias.o kgColor.o kgProtAlias.o kgXref.o knownInfo.o knownMore.o knownToSuper.o \
    lav.o ld.o ld2.o lfs.o liftOver.o liftOverChain.o liftUp.o \
    llaInfo.o longRange.o lrg.o lsSnpPdb.o lsSnpPdbChimera.o mafFrames.o mafGene.o mafSummary.o \
    makeItemsItem.o mammalPsg.o mapSts.o mathWig.o \
    mcnBreakpoints.o mdb.o metaChromGraph.o microarray.o \
    minChromSize.o minGeneInfo.o mrnaMisMatch.o \
    mouseOrtho.o mouseSyn.o mouseSynWhd.o mysqlTableStatus.o ncbiRefLink.o \
    ncbiRefSeqLink.o netAlign.o netCart.o nonCodingUi.o omimTitle.o ooUtils.o \
    orthoAlleles.o pal.o pbStamp.o pcrResult.o pepPred.o \
    peptideAtlasPeptide.o plasEndPairs.o polyGenotype.o protFeat.o pscreen.o \
    pseudoGeneLink.o pslReader.o pslWScore.o putaInfo.o qaSeq.o \
    rangeTreeFile.o rankProp.o recombRate.o recombRateRat.o recombRateMouse.o \
    refLink.o refSeqStatus.o \
    rgdQtl.o riken.o rhMapZfishInfo.o rikenBest.o rikenCluster.o rmskOut.o \
    rmskAlign.o rmskJoined.o rmskOut2.o \
    rnaFold.o rnaGene.o rnaGroup.o rnaHybridization.o rnaPLFold.o tRNAs.o gbRNAs.o snoRNAs.o lowelabPfamHit.o lowelabArkinOperonScore.o lowelabTIGROperonScore.o \
    rnaSecStr.o tfbsConsFactors.o \
    roughAli.o transMapStuff.o transMapInfo.o transMapGene.o transMapSrc.o \
    sage.o sageCounts.o sageExp.o samAlignment.o sample.o \
    sanger22extra.o sangerGene.o sangerGeneToWBGeneID.o  sargassoSeaXra.o \
    scopDes.o scoredRef.o search.o seqWindow.o sessionThumbnail.o sgdAbundance.o \
    sgdClone.o sgdDescription.o sgdOther.o simpleNucDiff.o simpleRepeat.o snakeUi.o \
    snp.o snp125.o snp125CodingCoordless.o snp132Ext.o snpExceptions.o snpFasta.o snpMap.o snpTmp.o \
    snpUi.o snp125Exceptions.o snp125Ui.o softPromoter.o softberryHom.o soTerm.o \
    spDb.o splignAlign.o sqlSanity.o stanMad.o straw.o stsAlias.o \
    stsInfo.o stsInfo2.o stsInfoMouse.o stsInfoMouseNew.o stsInfoRat.o \
    stsMap.o stsMapMouse.o stsMapMouseNew.o stsMapRat.o stsMarker.o suggest.o \
    switchDbTss.o synMap.o synteny100000.o syntenyBerk.o syntenySanger.o \
    sqlProg.o tfbsCons.o tfbsConsSites.o tablesTables.o \
    tableDescriptions.o tableStatus.o targetDb.o tfbsConsMap.o tagRepo.o \
    taxonDivision.o taxonGeneticCode.o taxonName.o taxonNode.o taxonXref.o \
    tigrCmrGene.o tigrOperon.o tilingPath.o traceInfo.o trackDb.o \
    trackDbCustom.o trackDbCache.o trackHub.o trackLayout.o trackTable.o trackVersion.o trashDir.o \
    transRegCode.o transRegCodeCondition.o \
    transRegCodeProbe.o txAliDiff.o txCluster.o txCommon.o txEdgeBed.o \
    txEdgeOrtho.o txGraph.o txInfo.o txRnaAccs.o ucscRetroInfo.o ucscRetroOrtho.o \
    validateGisaid.o variant.o variantProjector.o vcfUi.o \
    vegaInfo.o vegaInfoZfish.o visiGene.o vntr.o \
    wabAli.o web.o ncRna.o wgRna.o wigAsciiToBinary.o wigDataStream.o wiggle.o \
    wiggleCart.o wiggleUtils.o wikiLink.o wikiTrack.o yaleGencodeAssoc.o \
    zdobnovSynt.o oreganno.o \
    oregannoUi.o gvUi.o gv.o omicia.o protVar.o pgSnp.o \
    alignInfo.o cddInfo.o loweutils.o cddDesc.o arCOGs.o arcogdesc.o geneTree.o \
    megablastInfo.o pgPhenoAssoc.o pgSiftPred.o pgPolyphenPred.o userRegions.o variome.o

ifeq (${GBROWSE}, 1)
  GBROWSE_D=-DGBROWSE
else
  GBROWSE_D=
endif

%.o: %.c
	${CC} ${COPT} ${CFLAGS} ${GBROWSE_D} ${LOWELAB_DEFS} ${HG_DEFS} ${HG_WARN} ${HG_INC} ${XINC} -o $@ -c $<

../../lib/$(MACHTYPE)/jkhgap.a: $(O)
	ar rcus ../../lib/$(MACHTYPE)/jkhgap.a $(O)

jWestHeader.h: jWestHeader.html
	sed -e 's/\\/\\\\/g; s/"/\\"/g; s/^/"/; s/$$/\\n"/;' jWestHeader.html > jWestHeader.h

jWestBanner.h: jWestBanner.html
	sed -e 's/\\/\\\\/g; s/"/\\"/g; s/^/"/; s/$$/\\n"/;' jWestBanner.html > jWestBanner.h

web.o: jWestHeader.h jWestBanner.h

straw.o: straw/straw.cpp straw/straw.h
	cd straw && ${MAKE} straw.o

clean:
	rm -f $(O);
	rm -f jWestHeader.h jWestBanner.h
	rm -f ../../lib/$(MACHTYPE)/jkhgap.a;

ctags:
	ctags *.c *.h ../inc/*.h ../lib/*.c ../../inc/*.h ../../lib/*.c ../inc/encode/*.h ../lib/encode/*.c

tags:
	etags *.c *.h ../inc/*.h ../lib/*.c ../../inc/*.h ../../lib/*.c ../inc/encode/*.h ../lib/encode/*.c

test:
	cd tests && ${MAKE} test
