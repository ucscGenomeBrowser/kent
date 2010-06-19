include ../../inc/common.mk


O = toUpper.o 

toUpper: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/toUpper $O ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
