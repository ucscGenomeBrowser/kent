include ../../inc/common.mk


O = faGapSizes.o 

faGapSizes: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/faGapSizes $O ../../lib/$(MACHTYPE)/jkweb.a -lm

clean::
	rm -f ${O}
