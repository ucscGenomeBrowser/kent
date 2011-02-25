include ../../inc/common.mk


O = smithWat.o 

smithWat: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/smithWat $O ../../lib/$(MACHTYPE)/jkweb.a ${L} -lm

clean::
	rm -f ${O}
