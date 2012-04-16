include ../../inc/common.mk


O = catUncomment.o 

catUncomment: $(O)
	${CC} ${COPT} ${CFLAGS} -lm -o ${BINDIR}/catUncomment $O ../../lib/$(MACHTYPE)/jkweb.a ${L}
	${STRIP} ${BINDIR}/catUncomment${EXE}

clean::
	rm -f ${O}
