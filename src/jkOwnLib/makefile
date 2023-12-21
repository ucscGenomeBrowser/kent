kentSrc = ..
include ../inc/common.mk

O = bandExt.o crudeali.o ffAliHelp.o ffSeedExtend.o fuzzyFind.o \
    genoFind.o gfBlatLib.o gfClientLib.o gfInternal.o gfOut.o gfPcrLib.o gfWebLib.o ooc.o \
    patSpace.o splix.o supStitch.o trans3.o xenbig.o xensmall.o 

T = ../lib/$(MACHTYPE)/jkOwnLib.a

# for Huge-bit gfServer index
OHuge = $(addsuffix Huge.o, $(basename $O))
THuge = ../lib/$(MACHTYPE)/jkOwnLibHuge.a

%Huge.o: %.c
	${CC} -DGFSERVER_HUGE ${COPT} ${CFLAGS} ${HG_DEFS} ${LOWELAB_DEFS} ${HG_WARN} ${HG_INC} ${XINC} -o $@ -c $<

all: $(T) $(THuge)

$(T): $(O) ../lib/$(MACHTYPE)
	ar rcus $(T) $(O)

$(THuge): $(OHuge) ../lib/$(MACHTYPE)
	ar rcus $(THuge) $(OHuge)

../lib/$(MACHTYPE):
	mkdir ../lib/$(MACHTYPE)

clean:
	rm -f ${O} ${OHuge} ${T} ${THuge}

tags:
	etags ../inc/*.h ../lib/*.h ../lib/*.c  ../hg/inc/*.h ../hg/lib/*.h ../hg/lib/*.c ../hg/hgTracks/hgTracks.c ../hg/hgc/hgc.c ../hg/hgTrackUi/hgTrackUi.c
