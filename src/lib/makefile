include ../inc/common.mk


O = aliType.o apacheLog.o asParse.o axt.o axtAffine.o bamFile.o base64.o \
    basicBed.o bbiRead.o bbiWrite.o bigBed.o binRange.o bits.o blastOut.o \
    blastParse.o boxClump.o boxLump.o bPlusTree.o bwgCreate.o bwgQuery.o bwgValsOnChrom.o \
    cda.o chain.o chainBlock.o chainConnect.o chainToAxt.o chainToPsl.o \
    cheapcgi.o cirTree.o codebias.o colHash.o common.o correlate.o crTree.o \
    dgRange.o diGraph.o dlist.o dnaLoad.o dnaMarkov.o dnaMotif.o dnaseq.o \
    dnautil.o dtdParse.o dystring.o \
    emblParse.o errCatch.o errabort.o \
    fa.o ffAli.o ffScore.o filePath.o fixColor.o flydna.o fof.o \
    font/mgCourier10.o font/mgCourier12.o font/mgCourier14.o font/mgCourier18.o \
    font/mgCourier24.o font/mgCourier34.o font/mgCourier8.o font/mgHelvetica10.o \
    font/mgHelvetica12.o font/mgHelvetica14.o font/mgHelvetica18.o font/mgHelvetica24.o \
    font/mgHelvetica34.o font/mgHelvetica8.o font/mgHelveticaBold10.o font/mgHelveticaBold12.o \
    font/mgHelveticaBold14.o font/mgHelveticaBold18.o font/mgHelveticaBold24.o \
    font/mgHelveticaBold34.o font/mgHelveticaBold8.o font/mgSixhi6.o font/mgSail8.o \
    font/mgTimes10.o font/mgTimes12.o font/mgTimes14.o font/mgTimes18.o \
    font/mgTimes24.o font/mgTimes34.o font/mgTimes8.o \
    fuzzyShow.o \
    gapCalc.o gdf.o gemfont.o genomeRangeTree.o \
    gfNet.o gff.o gff3.o gfxPoly.o gifLabel.o \
    hacTree.o hash.o histogram.o hmmPfamParse.o hmmstats.o htmlPage.o htmshell.o \
    https.o intExp.o intValTree.o internet.o itsa.o \
    jointalign.o jpegSize.o keys.o knetUdc.o kxTok.o linefile.o localmem.o log.o \
    maf.o mafFromAxt.o mafScore.o md5.o memalloc.o memgfx.o metaWig.o mgCircle.o \
    mgPolygon.o mime.o net.o nib.o nibTwo.o nt4.o numObscure.o \
    obscure.o oldGff.o oligoTm.o options.o osunix.o pairHmm.o peakCluster.o \
    phyloTree.o pipeline.o portimpl.o pngwrite.o psGfx.o psPoly.o pscmGfx.o \
    psl.o pslGenoShow.o pslShow.o pslTbl.o pslTransMap.o pthreadWrap.o \
    qa.o quickHeap.o quotedP.o \
    ra.o rainbow.o rbTree.o rangeTree.o regexHelper.o repMask.o \
    rle.o rnautil.o rqlEval.o rqlParse.o rudp.o \
    scoreWindow.o seg.o seqOut.o seqStats.o servBrcMcw.o servCrunx.o \
    servcis.o servcl.o servmsII.o servpws.o shaRes.o slog.o snof.o \
    snofmake.o snofsig.o spaceSaver.o spacedColumn.o spacedSeed.o \
    splatAli.o sqlList.o sqlNum.o subText.o sufa.o sufx.o synQueue.o \
    tabRow.o textOut.o tokenizer.o trix.o twoBit.o \
    udc.o vcf.o vGfx.o vPng.o verbose.o \
    wildcmp.o wormdna.o \
    xAli.o xa.o xap.o xenshow.o xmlEscape.o xp.o zlibFace.o

$(MACHTYPE)/jkweb.a: $(O) $(MACHTYPE)
	ar rcus $(MACHTYPE)/jkweb.a $(O)

$(MACHTYPE):
	mkdir $(MACHTYPE)

test:
	cd tests && ${MAKE} test

clean:
	rm -f ${O} $(MACHTYPE)/jkweb.a
	cd tests && ${MAKE} clean

tags:
	etags ../inc/*.h ../lib/*.h ../lib/*.c  ../hg/inc/*.h ../hg/lib/*.h ../hg/lib/*.c ../hg/hgTracks/*.c ../hg/hgc/*.c ../hg/hgTrackUi/*.c
