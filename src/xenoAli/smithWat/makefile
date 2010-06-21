include ../../inc/common.mk


O = smithWat.o 

smithWat: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/smithWat $O ../../lib/$(MACHTYPE)/jkweb.a -lm

clean::
	rm -f ${O}
