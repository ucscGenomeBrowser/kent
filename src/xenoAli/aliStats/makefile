include ../../inc/common.mk


O = aliStats.o 

aliStats: $(O)
	${CC} ${COPT} ${CFLAGS} -o aliStats $O ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
