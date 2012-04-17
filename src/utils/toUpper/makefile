include ../../inc/common.mk


O = toUpper.o 

toUpper: $(O)
	${CC} ${COPT} ${CFLAGS} -lm -o ${BINDIR}/toUpper $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
