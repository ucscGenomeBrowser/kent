include ../../inc/common.mk


O = stitcher.o 

stitcher: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkOwnLib.a ../../lib/$(MACHTYPE)/jkweb.a -lm
	mv a.out $(HOME)/bin/$(MACHTYPE)/stitcher
