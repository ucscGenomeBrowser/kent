include ../../inc/common.mk


O = findCdna.o 

findCdna: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkweb.a $L
	mv ${AOUT} ${BINDIR}/findCdna

clean::
	rm -f ${O}
