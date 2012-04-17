include ../../inc/common.mk


O = toLower.o 

toLower: $(O)
	${CC} ${COPT} ${CFLAGS} -lm -o ${BINDIR}/toLower $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
