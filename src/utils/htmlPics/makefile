include ../../inc/common.mk


O = htmlPics.o 

htmlPics: $O
	${CC} ${COPT} ${CFLAGS} -lm -o ${BINDIR}/htmlPics $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
