include ../../inc/common.mk


O = geniegff.o 

geniegff: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/geniegff $O ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
