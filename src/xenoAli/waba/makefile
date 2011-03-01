include ../../inc/common.mk


O = waba.o wabaCrude.o

waba: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkOwnLib.a ../../lib/$(MACHTYPE)/jkweb.a ${L} -lm
	mv ${AOUT} ${BINDIR}/waba

clean::
	rm -f ${O}
