.c.o:
	gcc -D_GNU_SOURCE -g -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -c $*.c

O = aliType.o bits.o binRange.o \
    blastParse.o cda.o cheapcgi.o codebias.o common.o crudeali.o \
    dgRange.o diGraph.o dlist.o dnaMarkov.o \
    dnaseq.o dnautil.o dystring.o emblParse.o errabort.o \
    fa.o ffAli.o flydna.o fof.o fuzzyFind.o fuzzyShow.o gdf.o \
    gemfont.o genoFind.o gfClientLib.o gfNet.o gff.o \
    gifcomp.o gifwrite.o hash.o \
    hmmstats.o htmshell.o hugefnt.o intExp.o jointalign.o\
    keys.o kxTok.o largefnt.o linefile.o localmem.o \
    medfnt.o memalloc.o memgfx.o net.o nib.o \
    nt4.o obscure.o oldGff.o ooc.o options.o osunix.o patSpace.o \
    portimpl.o psl.o ra.o repMask.o rle.o sailfnt.o seqOut.o servcis.o \
    servCrunx.o servcl.o servmsII.o servpws.o shaRes.o \
    sixhi.o slog.o smallishfnt.o snof.o snofmake.o snofsig.o spaceSaver.o \
    sqlNum.o sqlList.o subText.o supStitch.o trans3.o wildcmp.o wormdna.o \
    xa.o xAli.o xap.o xenbig.o xenshow.o xensmall.o xp.o web.o 


$(MACHTYPE)/jkweb.a: $(O) $(MACHTYPE)
	ar rcu $(MACHTYPE)/jkweb.a $(O)
	ranlib $(MACHTYPE)/jkweb.a

$(MACHTYPE):
	mkdir $(MACHTYPE)

clean:
	rm -f ${O} $(MACHTYPE)/jkweb.a

tags:
	etags ../inc/*.h ../lib/*.h ../lib/*.c  ../hg/inc/*.h ../hg/lib/*.h ../hg/lib/*.c ../hg/hgTracks/hgTracks.c ../hg/hgc/hgc.c ../hg/hgTrackUi/hgTrackUi.c
