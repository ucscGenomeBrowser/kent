.c.o:
	gcc -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -c $*.c

O = crudeali.o fuzzyFind.o genoFind.o gfClientLib.o gfOut.o ooc.o \
patSpace.o supStitch.o trans3.o xenbig.o xensmall.o 

T = ../lib/$(MACHTYPE)/jkOwnLib.a

$(T): $(O) ../lib/$(MACHTYPE)
	ar rcu $(T) $(O)
	ranlib $(T)

../lib/$(MACHTYPE):
	mkdir ../lib/$(MACHTYPE)

clean:
	rm -f ${O} $(MACHTYPE)/jkOwnLib.a

tags:
	etags ../inc/*.h ../lib/*.h ../lib/*.c  ../hg/inc/*.h ../hg/lib/*.h ../hg/lib/*.c ../hg/hgTracks/hgTracks.c ../hg/hgc/hgc.c ../hg/hgTrackUi/hgTrackUi.c
