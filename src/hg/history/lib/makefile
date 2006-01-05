include ../../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

CFLAGS= -ggdb
O = element.o

../../../lib/$(MACHTYPE)/element.a: $(O) $(MACHTYPE)
	ar rcus $*.a $(O)

$(MACHTYPE):
	mkdir $(MACHTYPE)

test:
	cd tests && ${MAKE} test

clean:
	rm -f ${O} $(MACHTYPE)/element.a
	cd tests && ${MAKE} clean

tags:
	etags ../inc/*.h ../lib/*.h ../lib/*.c  ../hg/inc/*.h ../hg/lib/*.h ../hg/lib/*.c ../hg/hgTracks/*.c ../hg/hgc/*.c ../hg/hgTrackUi/*.c
