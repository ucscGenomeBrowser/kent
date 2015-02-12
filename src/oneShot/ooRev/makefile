include ../../inc/common.mk


O = ooRev.o 

ooRev: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/ooRev $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
