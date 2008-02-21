include ../inc/common.mk

O = aliType.o apacheLog.o asParse.o axt.o axtAffine.o \
    base64.o binRange.o bits.o blastOut.o blastParse.o boxClump.o \
    boxLump.o \
    cda.o chain.o chainBlock.o chainConnect.o chainToAxt.o chainToPsl.o \
    cheapcgi.o codebias.o colHash.o common.o correlate.o \
    dgRange.o diGraph.o dlist.o dnaLoad.o dnaMarkov.o dnaMotif.o dnaseq.o \
    dnautil.o dtdParse.o dystring.o \
    emblParse.o errCatch.o errabort.o \
    fa.o ffAli.o ffScore.o filePath.o fixColor.o flydna.o fof.o \
    fuzzyShow.o \
    gapCalc.o gdf.o gemfont.o gfNet.o gff.o gfxPoly.o gifLabel.o gifcomp.o \
    gifdecomp.o gifread.o gifwrite.o \
    hash.o histogram.o hmmPfamParse.o hmmstats.o htmlPage.o htmshell.o \
    intExp.o intValTree.o internet.o \
    jointalign.o jpegSize.o \
    keys.o kxTok.o \
    linefile.o localmem.o log.o \
    maf.o mafFromAxt.o mafScore.o md5.o memalloc.o memgfx.o mgCircle.o \
    mgHugeFont.o mgLargeFont.o mgMediumFont.o mgPolygon.o mgSmallFont.o \
    mgTinyFont.o mime.o \
    net.o nib.o nibTwo.o nt4.o \
    obscure.o oldGff.o oligoTm.o options.o osunix.o \
    pairHmm.o phyloTree.o pipeline.o portimpl.o psGfx.o psPoly.o pscmGfx.o \
    psl.o pslGenoShow.o pslShow.o pslTbl.o pslTransMap.o pthreadWrap.o \
    qa.o quickHeap.o quotedP.o \
    ra.o rangeTree.o rbTree.o repMask.o rle.o rnautil.o rudp.o \
    scoreWindow.o seg.o seqOut.o seqStats.o servBrcMcw.o servCrunx.o \
    servcis.o servcl.o servmsII.o servpws.o shaRes.o slog.o snof.o \
    snofmake.o snofsig.o spaceSaver.o spacedColumn.o spacedSeed.o \
    sqlList.o sqlNum.o subText.o synQueue.o \
    tabRow.o textOut.o tokenizer.o trix.o twoBit.o \
    vGfx.o vGif.o verbose.o \
    wildcmp.o wormdna.o \
    xAli.o xa.o xap.o xenshow.o xmlEscape.o xp.o

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
