include ../../inc/common.mk


O = fixInfo.o 

fixInfo: $(O)
	${CC} ${COPT} ${CFLAGS} -o fixInfo $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
