include ../../inc/common.mk


O = grokGff.o 

grokGff: $(O)
	${CC} ${COPT} ${CFLAGS} -o grokGff $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
