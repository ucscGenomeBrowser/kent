kentSrc = ..
include ../inc/common.mk

O = bandExt.o crudeali.o ffAliHelp.o ffSeedExtend.o fuzzyFind.o \
    genoFind.o gfBlatLib.o gfClientLib.o gfInternal.o gfOut.o gfPcrLib.o gfWebLib.o ooc.o \
    patSpace.o splix.o supStitch.o trans3.o xenbig.o xensmall.o 

T = ../lib/$(MACHTYPE)/jkOwnLib.a

# for 64-bit gfServer index
O64 = $(addsuffix 64.o, $(basename $O))
T64 = ../lib/$(MACHTYPE)/jkOwnLib64.a

%64.o: %.c
	${CC} -DGFSERVER64 ${COPT} ${CFLAGS} ${HG_DEFS} ${LOWELAB_DEFS} ${HG_WARN} ${HG_INC} ${XINC} -o $@ -c $<

all: $(T) $(T64)

$(T): $(O) ../lib/$(MACHTYPE)
	ar rcus $(T) $(O)

$(T64): $(O64) ../lib/$(MACHTYPE)
	ar rcus $(T64) $(O64)

../lib/$(MACHTYPE):
	mkdir ../lib/$(MACHTYPE)

clean:
	rm -f ${O} ${O64} ${T} ${T64}

tags:
	etags ../inc/*.h ../lib/*.h ../lib/*.c  ../hg/inc/*.h ../hg/lib/*.h ../hg/lib/*.c ../hg/hgTracks/hgTracks.c ../hg/hgc/hgc.c ../hg/hgTrackUi/hgTrackUi.c
