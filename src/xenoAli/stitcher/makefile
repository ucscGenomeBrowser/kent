include ../../inc/common.mk


O = stitcher.o 

stitcher: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkOwnLib.a ../../lib/$(MACHTYPE)/jkweb.a ${L} -lm
	mv ${AOUT} ${BINDIR}/stitcher

clean::
	rm -f "${O}" "${AOUT}"
