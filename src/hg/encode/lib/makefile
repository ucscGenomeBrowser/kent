include ../../../inc/common.mk


O = encodePatchTdb.o

$(MACHTYPE)/encode.a: $(O) $(MACHTYPE)
	ar rcus $(MACHTYPE)/encode.a $(O)

$(MACHTYPE):
	mkdir $(MACHTYPE)

test:
	cd tests && ${MAKE} test

clean:
	rm -f ${O} $(MACHTYPE)/encode.a
	cd tests && ${MAKE} clean

tags:
	etags ../inc/*.h ../lib/*.h ../lib/*.c  ../hg/inc/*.h ../hg/lib/*.h ../hg/lib/*.c ../hg/hgTracks/*.c ../hg/hgc/*.c ../hg/hgTrackUi/*.c
