.c.o:
	gcc -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -c $*.c

O = aliType.o axt.o bits.o binRange.o \
    blastOut.o blastParse.o boxClump.o \
    cda.o chainBlock.o cheapcgi.o codebias.o common.o \
    dgRange.o diGraph.o dlist.o dnaMarkov.o \
    dnaseq.o dnautil.o dystring.o emblParse.o errabort.o \
    fa.o ffAli.o fixColor.o flydna.o fof.o fuzzyShow.o gdf.o \
    gemfont.o gfNet.o gff.o \
    gifcomp.o gifdecomp.o gifread.o gifwrite.o hash.o \
    hmmstats.o htmshell.o hugefnt.o intExp.o jointalign.o\
    keys.o kxTok.o largefnt.o linefile.o localmem.o \
    maf.o mafFromAxt.o medfnt.o memalloc.o memgfx.o net.o nib.o \
    nt4.o obscure.o oldGff.o options.o osunix.o \
    portimpl.o pscmGfx.o psGfx.o psl.o ra.o rbTree.o \
    repMask.o rle.o sailfnt.o seqOut.o servcis.o \
    servCrunx.o servcl.o servmsII.o servpws.o shaRes.o \
    sixhi.o slog.o smallishfnt.o snof.o snofmake.o snofsig.o spaceSaver.o \
    sqlNum.o sqlList.o subText.o vGfx.o vGif.o wildcmp.o wormdna.o \
    xa.o xAli.o xap.o xenshow.o xp.o


$(MACHTYPE)/jkweb.a: $(O) $(MACHTYPE)
	ar rcus $(MACHTYPE)/jkweb.a $(O)

$(MACHTYPE):
	mkdir $(MACHTYPE)

clean:
	rm -f ${O} $(MACHTYPE)/jkweb.a

tags:
	etags ../inc/*.h ../lib/*.h ../lib/*.c  ../hg/inc/*.h ../hg/lib/*.h ../hg/lib/*.c ../hg/hgTracks/hgTracks.c ../hg/hgc/hgc.c ../hg/hgTrackUi/hgTrackUi.c
