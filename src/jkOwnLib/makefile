include ../inc/common.mk

O = bandExt.o crudeali.o ffAliHelp.o ffSeedExtend.o fuzzyFind.o \
    genoFind.o gfBlatLib.o gfClientLib.o gfInternal.o gfOut.o gfPcrLib.o gfWebLib.o ooc.o \
    patSpace.o splix.o supStitch.o trans3.o xenbig.o xensmall.o 

T = ../lib/$(MACHTYPE)/jkOwnLib.a

$(T): $(O) ../lib/$(MACHTYPE)
	ar rcus $(T) $(O)

../lib/$(MACHTYPE):
	mkdir ../lib/$(MACHTYPE)

clean:
	rm -f ${O} ../lib/$(MACHTYPE)/jkOwnLib.a

tags:
	etags ../inc/*.h ../lib/*.h ../lib/*.c  ../hg/inc/*.h ../hg/lib/*.h ../hg/lib/*.c ../hg/hgTracks/hgTracks.c ../hg/hgc/hgc.c ../hg/hgTrackUi/hgTrackUi.c
