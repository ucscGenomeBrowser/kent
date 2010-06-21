include ../../inc/common.mk


O = m22dirs.o 

m22dirs: $(O)
	${CC} ${COPT} ${CFLAGS} -o m22dirs $O ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
