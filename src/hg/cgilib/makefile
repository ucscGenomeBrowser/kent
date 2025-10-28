kentSrc = ../..
include ../../inc/localEnvironment.mk
include ../../inc/common.mk
XINC = -I$(MYSQLINC)

O=HInv.o affy10KDetails.o affy120KDetails.o ancientRref.o annoFormatVep.o annoGrateWigDb.o annoGratorGpVar.o \
  annoStreamDb.o annoStreamDbFactorSource.o annoStreamDbPslPlus.o annoStreamWig.o api.o atomDb.o axtLib.o \
  bactigPos.o barChartData.o barChartSample.o bdgpExprLink.o bdgpGeneInfo.o bed5FloatScore.o \
  bed6FloatScore.o bed8Attrs.o bedCart.o bgiGeneInfo.o bgiGeneSnp.o bgiSnp.o bigTransMap.o borkPseudoHom.o \
  cartEdit0.o cartEdit1.o cartEdit2.o cartEdit3.o cartEdit4.o cartEdit5.o cartEdit6.o cartEdit7.o \
  cartEdit9.o cartJson.o cartRewrite.o ccdsGeneMap.o ccdsInfo.o ccdsNotes.o \
  cdsSpec.o celeraCoverage.o celeraDupPositive.o cgh.o chicken13kInfo.o chr18deletions.o cnpIafrate.o \
  cnpIafrate2.o cnpLocke.o cnpSebat.o cnpSebat2.o cnpSharp.o cnpSharp2.o codeBlast.o cogs.o contigAcc.o \
  cpgIslandExt.o ctgPos2.o cutter.o dbRIP.o dbSnpDetails.o dbSnpRs.o delConrad2.o delHinds2.o dgv.o \
  dgvPlus.o dless.o dnaProbe.o dv.o dvBed.o dvXref2.o ensFace.o ensInfo.o ensPhusionBlast.o est3.o \
  factorSource.o fbTables.o fileUi.o fishClones.o flyBase2004Xref.o flyBaseSwissProt.o flyreg.o flyreg2.o \
  gbMiscDiff.o gbProtAnn.o gbWarn.o genMapDb.o gencodeAnnotationRemark.o gencodeExonSupport.o \
  gencodeGeneSource.o gencodeIntron.o gencodeTag.o gencodeToEntrezGene.o gencodeToPdb.o gencodeToPubMed.o \
  gencodeToUniProt.o gencodeTranscriptSource.o gencodeTranscriptSupport.o gencodeTranscriptionSupportLevel.o \
  geneSimilarities.o genoLay.o genomicDups.o genomicSuperDups.o growthCondition.o gtexBoxplot.o \
  gtexEqtlCluster.o gv.o gvUi.o gwasCatalog.o hAnno.o hCytoBand.o haplotypes.o hapmapAllelesSummary.o \
  hashJoin.o hgBam.o hgExp.o hgSeq.o hgdpGeo.o humanParalog.o hvGfx.o isochores.o itemConf.o \
  itemDetailsHtml.o jaxOrtholog.o jaxQTL.o jaxQTL3.o joinMixer.o kg1ToKg2.o ld.o ld2.o lsSnpPdbChimera.o \
  mammalPsg.o mapSts.o mathWig.o mcnBreakpoints.o mouseOrtho.o mouseSyn.o mouseSynWhd.o ncRna.o \
  ncbiRefSeqLink.o nonCodingUi.o omicia.o omimTitle.o oreganno.o oregannoUi.o pal.o pcrResult.o \
  peptideAtlasPeptide.o pgPolyphenPred.o pgSiftPred.o polyGenotype.o protVar.o protobuf.o pscreen.o \
  pseudoGeneLink.o pslWScore.o putaInfo.o recombRate.o recombRateMouse.o recombRateRat.o rhMapZfishInfo.o \
  rnaFold.o rnaHybridization.o rnaPLFold.o rnaSecStr.o roughAli.o sageExp.o \
  search.o sessionThumbnail.o sgdClone.o sgdDescription.o simpleNucDiff.o simpleRepeat.o snp125.o \
  snp125CodingCoordless.o snp125Ui.o snp132Ext.o snpMap.o snpUi.o softPromoter.o softberryHom.o stsInfo.o \
  stsInfoMouse.o stsInfoMouseNew.o stsInfoRat.o stsMapMouse.o stsMapMouseNew.o stsMapRat.o suggest.o \
  switchDbTss.o synteny100000.o tRNAs.o targetDb.o tfbsConsFactors.o tfbsConsMap.o tfbsConsSites.o \
  trackLayout.o trackTable.o transMapGene.o transMapInfo.o transMapSrc.o transMapStuff.o transRegCode.o \
  transRegCodeProbe.o ucscRetroInfo.o ucscRetroOrtho.o userRegions.o variant.o variome.o vegaInfoZfish.o \
  visiGene.o vntr.o wabAli.o wgRna.o yaleGencodeAssoc.o zdobnovSynt.o

ifeq (${GBROWSE}, 1)
  GBROWSE_D=-DGBROWSE
else
  GBROWSE_D=
endif

%.o: %.c
	${CC} ${COPT} ${CFLAGS} ${GBROWSE_D} ${LOWELAB_DEFS} ${HG_DEFS} ${HG_WARN} ${HG_INC} ${XINC} -o $@ -c $<

../../lib/$(MACHTYPE)/jkhgapcgi.a: $(O)
	ar rcus ../../lib/$(MACHTYPE)/jkhgapcgi.a $(O)

clean:
	rm -f $(O);
	rm -f ../../lib/$(MACHTYPE)/jkhgapcgi.a;

ctags:
	ctags *.c *.h ../inc/*.h ../lib/*.c ../../inc/*.h ../../lib/*.c ../inc/encode/*.h ../lib/encode/*.c

tags:
	etags *.c *.h ../inc/*.h ../lib/*.c ../../inc/*.h ../../lib/*.c ../inc/encode/*.h ../lib/encode/*.c

test:
	cd tests && ${MAKE} test
