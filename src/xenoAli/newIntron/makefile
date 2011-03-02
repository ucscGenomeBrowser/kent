include ../../inc/common.mk


O = newIntron.o 

newIntron: $(O)
	${CC} ${COPT} ${CFLAGS} -o newIntron $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
