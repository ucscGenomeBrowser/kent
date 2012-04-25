include ../../inc/common.mk


O = bwana.o 

bwana: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkOwnLib.a ../../lib/$(MACHTYPE)/jkweb.a ${L}
	mv ${AOUT} ${BINDIR}/bwana

clean::
	rm -f "${O}" "${AOUT}"
