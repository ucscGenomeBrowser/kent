.c.o:
	gcc -Wimplicit -ggdb -O -I../inc -c $*.c

O = bits.o blastParse.o cda.o cheapcgi.o codebias.o common.o crudeali.o \
    dgRange.o diGraph.o dlist.o dnaseq.o dnautil.o dystring.o errabort.o \
    fa.o ffAli.o flydna.o fof.o fuzzyFind.o fuzzyShow.o gdf.o \
    gemfont.o genoFind.o gfClientLib.o gff.o \
    gifcomp.o gifwrite.o hash.o \
    hmmstats.o htmshell.o hugefnt.o \
    keys.o largefnt.o linefile.o localmem.o \
    medfnt.o memalloc.o memgfx.o nib.o \
    nt4.o obscure.o oldGff.o ooc.o osunix.o patSpace.o \
    portimpl.o psl.o rle.o sailfnt.o seqOut.o servcis.o \
    servCrunx.o servcl.o servmsII.o servpws.o shaRes.o \
    sixhi.o smallishfnt.o snof.o snofmake.o snofsig.o spaceSaver.o \
    sqlNum.o sqlList.o  supStitch.o trans3.o wildcmp.o wormdna.o \
    xa.o xenbig.o xenshow.o xensmall.o


$(MACHTYPE)/jkweb.a: $(O) 
	ar rcu $(MACHTYPE)/jkweb.a $(O)
	ranlib $(MACHTYPE)/jkweb.a
