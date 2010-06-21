include ../../inc/common.mk


O = fixInfo.o 

fixInfo: $(O)
	${CC} ${COPT} ${CFLAGS} -o fixInfo $O ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
