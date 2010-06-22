include ../../inc/common.mk


O = fixCr.o 

fixCr: $O
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/fixCr $O ../../lib/$(MACHTYPE)/jkweb.a 
	${STRIP} ${BINDIR}/fixCr${EXE}

clean::
	rm -f ${O}
