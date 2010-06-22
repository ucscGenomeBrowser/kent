include ../../inc/common.mk


O = htmlPics.o 

htmlPics: $O
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/htmlPics $O ../../lib/$(MACHTYPE)/jkweb.a 

clean::
	rm -f ${O}
