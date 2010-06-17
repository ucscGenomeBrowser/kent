include ../../inc/common.mk


O = fatont4.o 

fatont4: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkweb.a
	mv ${AOUT} ${BINDIR}/fatont4

clean::
	rm -f ${O}
