include ../../inc/common.mk
XINC = -I$(MYSQLINC)
# MJP: -pg for gprof output 
#CFLAGS = -ggdb -pg 
CFLAGS = -ggdb

L = -lm $(MYSQLLIBS)

O = acemblyClass.o affyAllExonProbe.o affyAtlas.o affy10KDetails.o affy120KDetails.o \
    affyOffset.o affyPairs.o agp.o agpFrag.o agpGap.o alignSeqSizes.o altGraph.o \
    altGraphX.o ancientRref.o atomDb.o  axtInfo.o \
    axtLib.o bactigPos.o baseMaskCommon.o bdgpExprLink.o bdgpGeneInfo.o \
    bed.o bed5FloatScore.o bed5Pval.o bed6FloatScore.o bed12Source.o \
    bedCart.o bgiGeneInfo.o bgiGeneSnp.o bgiSnp.o bioImage.o \
    blastTab.o blastzNet.o blatServers.o borf.o borkPseudoHom.o botDelay.o\
    cart.o cartDb.o cdsEvidence.o cdsOrtho.o cdsPick.o cdsSpec.o \
    ccdsInfo.o ccdsGeneMap.o celeraCoverage.o \
    celeraDupPositive.o cgapSage/cgapSage.o cgapSage/cgapSageLib.o cgh.o chainCart.o \
    chainDb.o chainLink.o chainNet.o chainNetDbLoad.o \
    chicken13kInfo.o chromBins.o chr18deletions.o \
    chromGraph.o chromGraphFactory.o chromInfo.o chromInserts.o chromKeeper.o \
    clonePos.o codeBlast.o codeBlastScore.o cogs.o cogsxra.o columnInfo.o \
    contigAcc.o coordConv.o \
    cnpIafrate.o cnpIafrate2.o cnpLocke.o cnpRedon.o cnpSebat.o cnpSebat2.o \
    cnpSharp2.o cnpSharpCutoff.o cnpSharpSample.o cnpSharp.o \
    cpgIsland.o cpgIslandExt.o ctgPos.o ctgPos2.o \
    customFactory.o customPp.o customTrack.o cutter.o cytoBand.o \
    dbDb.o dbRIP.o dbSnpRs.o defaultDb.o delConrad2.o delHinds2.o \
    dgv.o dless.o dnaMotifSql.o dnaProbe.o \
    dv.o dvBed.o dvXref2.o \
    easyGene.o ec.o ecCode.o ecAttribute.o ecAttributeCode.o \
    encode/encodeErge.o encode/encodeErgeHssCellLines.o \
    encode/encodeHapMapAlleleFreq.o encode/encodeIndels.o encode/encodePeak.o \
    encode/encodeRegionInfo.o encode/encodeRegionInfoCustom.o encode/encodeRna.o \
    encode/encodeStanfordPromoters.o encode/encodeStanfordPromotersAverage.o \
    ensFace.o ensInfo.o ensPhusionBlast.o ensXRefZfish.o \
    est3.o estOrientInfo.o \
    estPair.o exoFish.o expData.o expRecord.o exprBed.o \
    fbTables.o featureBits.o findKGAlias.o findKGProtAlias.o fishClones.o \
    flyBase2004Xref.o \
    flyBaseSwissProt.o flyreg.o flyreg2.o gbExtFile.o gbWarn.o gbMiscDiff.o gbProtAnn.o gcPercent.o \
    genbank.o gencodeGeneClass.o gencodeIntron.o genMapDb.o \
    geneBands.o geneCheck.o geneCheckDetails.o geneCheckWidget.o \
    geneGraph.o genePred.o genePredReader.o geneSimilarities.o genoLay.o \
    genomeRangeTreeFile.o genomicDups.o \
    genomicSuperDups.o ggCluster.o ggDbIo.o ggDump.o ggGraph.o ggMrnaAli.o \
    ggTypes.o glDbRep.o goa.o goaPart.o googleAnalytics.o \
    growthCondition.o grp.o \
    hapmapAllelesOrtho.o hapmapAllelesSummary.o \
    hapmapSnps.o hapmapSnpsCombined.o \
    hCommon.o \
    hCytoBand.o hdb.o hgColors.o hgConfig.o hgExp.o hgFind.o \
    hgFindSpec.o hgFindSpecCustom.o \
    hgGene.o hgMaf.o hgRelate.o hgSeq.o hPrint.o  hvGfx.o \
    HInv.o hui.o humanParalog.o \
    imageClone.o isochores.o ispyTables.o itemAttr.o itemConf.o jalview.o \
    jaxOrtholog.o jaxQTL.o jaxQTL3.o jksql.o joiner.o jsHelper.o kg1ToKg2.o \
    jgiGene.o \
    kgAlias.o kgColor.o kgProtAlias.o kgXref.o knownInfo.o knownMore.o knownToSuper.o \
    ld.o ld2.o lfs.o liftOver.o liftOverChain.o liftUp.o \
    llaInfo.o mafFrames.o mafGene.o mafSummary.o mammalPsg.o mapSts.o \
    mcnBreakpoints.o metaChromGraph.o microarray.o \
    minChromSize.o minGeneInfo.o mrnaMisMatch.o \
    mouseOrtho.o mouseSyn.o mouseSynWhd.o mysqlTableStatus.o \
    netAlign.o nonCodingUi.o omimTitle.o ooUtils.o orthoAlleles.o \
    pal.o pbStamp.o pcrResult.o pepPred.o plasEndPairs.o \
    polyGenotype.o protFeat.o pscreen.o \
    pseudoGeneLink.o pslReader.o pslWScore.o putaInfo.o qaSeq.o \
    rangeTreeFile.o rankProp.o recombRate.o recombRateRat.o recombRateMouse.o \
    refLink.o refSeqStatus.o \
    rgdQtl.o rgdQtl.o rgdQtl.o rgdQtl.o rgdQtl.o rgdQtl.o rgdQtl.o \
    rgdQtl.o riken.o rhMapZfishInfo.o rikenBest.o rikenCluster.o rmskOut.o \
    rnaFold.o rnaGene.o rnaGroup.o rnaHybridization.o rnaPLFold.o tRNAs.o gbRNAs.o snoRNAs.o lowelabPfamHit.o lowelabArkinOperonScore.o lowelabTIGROperonScore.o \
    rnaSecStr.o tfbsConsFactors.o retroMrnaInfo.o \
    roughAli.o transMapStuff.o transMapInfo.o transMapGene.o transMapSrc.o sage.o sageCounts.o sageExp.o sample.o \
    sqlProg.o tfbsCons.o tfbsConsSites.o \
    sanger22extra.o sangerGene.o sangerGeneToWBGeneID.o  sargassoSeaXra.o \
    scopDes.o scoredRef.o sgdAbundance.o \
    sgdClone.o sgdDescription.o sgdOther.o simpleNucDiff.o simpleRepeat.o \
    snp.o snp125.o snpExceptions.o snpFasta.o snpMap.o snpTmp.o \
    snpUi.o snp125Exceptions.o snp125Ui.o softPromoter.o softberryHom.o \
    spDb.o splignAlign.o stanMad.o stsAlias.o \
    stsInfo.o stsInfo2.o stsInfoMouse.o stsInfoMouseNew.o stsInfoRat.o \
    stsMap.o stsMapMouse.o stsMapMouseNew.o stsMapRat.o stsMarker.o \
    switchDbTss.o synMap.o synteny100000.o syntenyBerk.o syntenySanger.o \
    tableDescriptions.o tableStatus.o targetDb.o tfbsConsMap.o \
    taxonDivision.o taxonGeneticCode.o taxonName.o taxonNode.o taxonXref.o \
    tigrCmrGene.o tigrOperon.o tilingPath.o traceInfo.o trackDb.o \
    trackDbCustom.o trackLayout.o trackTable.o trackVersion.o trashDir.o \
    transRegCode.o transRegCodeCondition.o \
    transRegCodeProbe.o txCluster.o txCommon.o txEdgeBed.o \
    txEdgeOrtho.o txGraph.o txInfo.o txRnaAccs.o \
    vegaInfo.o vegaInfoZfish.o visiGene.o vntr.o \
    wabAli.o web.o ncRna.o wgRna.o wigAsciiToBinary.o wigDataStream.o wiggle.o \
    wiggleCart.o wiggleUtils.o wikiLink.o wikiTrack.o zdobnovSynt.o oreganno.o \
    oregannoUi.o gvUi.o gv.o omicia.o protVar.o pgSnp.o


ifeq (${GBROWSE}, 1)
  GBROWSE_D=-DGBROWSE
else
  GBROWSE_D=
endif

%.o: %.c
	${CC} ${COPT} ${CFLAGS} ${GBROWSE_D} ${HG_DEFS} ${HG_WARN} ${HG_INC} ${XINC} -o $@ -c $<

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
