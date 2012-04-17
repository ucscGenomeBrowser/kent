include ../../inc/common.mk


O = fixCr.o 

fixCr: $O
	${CC} ${COPT} ${CFLAGS} -lm -o ${BINDIR}/fixCr $O ../../lib/$(MACHTYPE)/jkweb.a ${L}
	${STRIP} ${BINDIR}/fixCr${EXE}

clean::
	rm -f ${O}
