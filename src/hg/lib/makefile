include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}
XINC = -I$(MYSQLINC)

L = -lm $(MYSQLLIBS)

O = acemblyClass.o affyAllExonProbe.o affyAtlas.o affy10KDetails.o affy120KDetails.o \
    affyOffset.o affyPairs.o agp.o agpFrag.o agpGap.o alignSeqSizes.o altGraph.o \
    altGraphX.o ancientRref.o  axtInfo.o \
    axtLib.o bactigPos.o bdgpExprLink.o bdgpGeneInfo.o \
    bed.o bed5FloatScore.o bed6FloatScore.o bed12Source.o \
    bedCart.o bgiGeneInfo.o bgiGeneSnp.o bgiSnp.o bioImage.o \
    blastTab.o blastzNet.o blatServers.o borf.o borkPseudoHom.o botDelay.o \
    cart.o cartDb.o ccdsInfo.o ccdsGeneMap.o celeraCoverage.o \
    celeraDupPositive.o cgh.o chainCart.o \
    chainDb.o chainLink.o chainNet.o chainNetDbLoad.o \
    chicken13kInfo.o chromBins.o chr18deletions.o \
    chromGraph.o chromInfo.o chromInserts.o chromKeeper.o \
    clonePos.o codeBlast.o codeBlastScore.o cogs.o cogsxra.o columnInfo.o \
    contigAcc.o coordConv.o \
    cnpIafrate.o cnpSebat.o cnpSharpCutoff.o cnpSharpSample.o cnpSharp.o \
    cpgIsland.o cpgIslandExt.o ctgPos.o ctgPos2.o \
    customTrack.o cutter.o cytoBand.o \
    dbDb.o dbSnpRs.o defaultDb.o dless.o dnaMotifSql.o dnaProbe.o \
    dv.o dvBed.o dvXref2.o \
    easyGene.o ec.o ecCode.o ecAttribute.o ecAttributeCode.o \
    encodeErge.o encodeErgeHssCellLines.o \
    encodeHapMapAlleleFreq.o encodeIndels.o \
    encodeRegionInfo.o encodeRegionInfoCustom.o encodeRna.o \
    encodeStanfordPromoters.o encodeStanfordPromotersAverage.o \
    ensFace.o ensPhusionBlast.o \
    est3.o estOrientInfo.o \
    estPair.o exoFish.o expData.o expRecord.o exprBed.o \
    fbTables.o featureBits.o findKGAlias.o findKGProtAlias.o fishClones.o \
    flyBase2004Xref.o \
    flyBaseSwissProt.o flyreg.o flyreg2.o gbExtFile.o gbProtAnn.o gcPercent.o \
    gencodeGeneClass.o gencodeIntron.o genMapDb.o genbank.o \
    geneBands.o geneCheck.o geneCheckDetails.o geneCheckWidget.o geneGraph.o genePred.o genePredReader.o genomicDups.o \
    genomicSuperDups.o ggCluster.o ggDbIo.o ggDump.o ggGraph.o ggMrnaAli.o \
    glDbRep.o goa.o goaPart.o growthCondition.o grp.o hapmapSnps.o hCommon.o \
    hdb.o hgConfig.o hgExp.o hgFind.o hgFindSpec.o hgFindSpecCustom.o \
    hgGene.o hgMaf.o hgMut.o hgMutUi.o hgRelate.o hgSeq.o \
    HInv.o hui.o humanParalog.o humPhen.o \
    imageClone.o isochores.o itemAttr.o jalview.o \
    jaxOrtholog.o jaxQTL.o jaxQTL3.o jksql.o joiner.o \
    kgAlias.o kgProtAlias.o kgXref.o knownInfo.o knownMore.o knownToSuper.o \
    ld.o ldUi.o lfs.o liftOver.o liftOverChain.o liftUp.o \
    llaInfo.o mafFrames.o mafSummary.o mapSts.o \
    mcnBreakpoints.o metaChromGraph.o minGeneInfo.o mrnaMisMatch.o \
    mouseOrtho.o mouseSyn.o mouseSynWhd.o mysqlTableStatus.o \
    netAlign.o omimTitle.o ooUtils.o \
    pbStamp.o pepPred.o plasEndPairs.o pscreen.o \
    pseudoGeneLink.o pslReader.o pslWScore.o putaInfo.o qaSeq.o \
    rankProp.o recombRate.o recombRateRat.o recombRateMouse.o \
    refLink.o refSeqStatus.o \
    rgdQtl.o rgdQtl.o rgdQtl.o rgdQtl.o rgdQtl.o rgdQtl.o rgdQtl.o rgdQtl.o riken.o rikenBest.o rikenCluster.o rmskOut.o \
    rnaFold.o rnaGene.o rnaGroup.o tRNAs.o gbRNAs.o \
    rnaSecStr.o tfbsConsFactors.o \
    roughAli.o transMapInfo.o sage.o sageCounts.o sageExp.o sample.o \
    sqlProg.o tfbsCons.o tfbsConsSites.o \
    sanger22extra.o sangerGene.o sargassoSeaXra.o \
    scopDes.o scoredRef.o sgdAbundance.o \
    sgdClone.o sgdDescription.o sgdOther.o simpleNucDiff.o simpleRepeat.o \
    snp.o snp125.o snpExceptions.o snpFasta.o snpMap.o snpTmp.o \
    snpUi.o snp125Exceptions.o snp125Ui.o softPromoter.o softberryHom.o \
    spDb.o splignAlign.o stanMad.o stsAlias.o \
    stsInfo.o stsInfo2.o stsInfoMouse.o stsInfoMouseNew.o stsInfoRat.o \
    stsMap.o stsMapMouse.o stsMapMouseNew.o stsMapRat.o stsMarker.o \
    synMap.o synteny100000.o syntenyBerk.o syntenySanger.o \
    tableDescriptions.o tableStatus.o tfbsConsMap.o \
    taxonDivision.o taxonGeneticCode.o taxonName.o taxonNode.o taxonXref.o \
    tigrCmrGene.o tigrOperon.o tilingPath.o traceInfo.o trackDb.o \
    trackDbCustom.o trackLayout.o trackTable.o \
    transRegCode.o transRegCodeCondition.o \
    transRegCodeProbe.o vegaInfo.o visiGene.o vntr.o wabAli.o web.o wgRna.o \
    wigAsciiToBinary.o wigDataStream.o wiggle.o wiggleCart.o wiggleUtils.o \
    wikiLink.o zdobnovSynt.o landmark.o landmarkUi.o genomeVarUi.o genomeVar.o


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
	cd tests && ${MAKE} test
