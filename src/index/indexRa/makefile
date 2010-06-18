include ../../inc/common.mk


O = indexRa.o 

indexRa: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkweb.a
	mv ${AOUT} ${BINDIR}/indexRa

clean::
	rm -f ${O}
