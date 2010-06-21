include ../../inc/common.mk


O = nibSize.o 

nibSize: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/nibSize $O ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
