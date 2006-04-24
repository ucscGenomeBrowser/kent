include ../inc/common.mk
#HG_WARN = ${HG_WARN_ERR}

O = aliType.o apacheLog.o asParse.o axt.o axtAffine.o \
    base64.o bits.o binRange.o \
    blastOut.o blastParse.o boxClump.o boxLump.o browserGfx.o \
    cda.o chain.o chainBlock.o chainConnect.o chainToAxt.o \
    chainToPsl.o cheapcgi.o codebias.o colHash.o common.o \
    dgRange.o diGraph.o dlist.o dnaLoad.o dnaMarkov.o \
    dnaseq.o dnautil.o dnaMotif.o dtdParse.o dystring.o \
    emblParse.o errabort.o errCatch.o \
    fa.o ffAli.o ffScore.o filePath.o fixColor.o flydna.o fof.o \
    fuzzyShow.o gapCalc.o gdf.o gemfont.o gfNet.o gff.o gfxPoly.o \
    gifcomp.o gifdecomp.o gifLabel.o gifread.o gifwrite.o hash.o \
    histogram.o hmmstats.o htmlPage.o htmshell.o \
    internet.o intExp.o jointalign.o jpegSize.o \
    keys.o kxTok.o linefile.o localmem.o log.o \
    maf.o mafFromAxt.o mafScore.o md5.o \
    mgTinyFont.o mgSmallFont.o mgMediumFont.o mgLargeFont.o mgHugeFont.o \
    memalloc.o memgfx.o mgCircle.o mgPolygon.o mime.o net.o nib.o nibTwo.o \
    nt4.o obscure.o oldGff.o oligoTm.o options.o osunix.o pairHmm.o phyloTree.o \
    pipeline.o portimpl.o pscmGfx.o psGfx.o psl.o pslGenoShow.o \
    pslShow.o pslTbl.o pslTransMap.o pthreadWrap.o qa.o quickHeap.o ra.o rbTree.o \
    repMask.o rle.o rnautil.o rudp.o scoreWindow.o \
    seqOut.o seqStats.o servBrcMcw.o servcis.o \
    servCrunx.o servcl.o servmsII.o servpws.o shaRes.o \
    slog.o snof.o snofmake.o snofsig.o \
    spacedSeed.o spaceSaver.o \
    sqlNum.o sqlList.o subText.o synQueue.o textOut.o tokenizer.o trix.o \
    twoBit.o verbose.o vGfx.o vGif.o wildcmp.o wormdna.o \
    xa.o xAli.o xap.o xenshow.o xmlEscape.o xp.o 


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
