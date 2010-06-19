include ../../inc/common.mk


O = faGapLocs.o 

faGapLocs: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/faGapLocs $O ../../lib/$(MACHTYPE)/jkweb.a -lm

clean::
	rm -f ${O}
