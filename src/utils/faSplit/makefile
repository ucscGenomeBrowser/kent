include ../../inc/common.mk

CFLAGS=-ggdb

O = faSplit.o 

faSplit: ${O}
	${CC} ${COPT} -ggdb -o ${BINDIR}/faSplit $O \
	../../lib/${MACHTYPE}/jkweb.a ${L}
	${STRIP} ${BINDIR}/faSplit${EXE}

debug:
	${CC} ${COPT} -o faSplit $O ../../lib/${MACHTYPE}/jkweb.a ${L}

clean::
	rm -f ${O} faSplit


