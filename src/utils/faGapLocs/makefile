include ../../inc/common.mk


O = faGapLocs.o 

faGapLocs: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/faGapLocs $O ../../lib/$(MACHTYPE)/jkweb.a ${L} -lm

clean::
	rm -f ${O}
