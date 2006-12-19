include ../../inc/common.mk
HG_WARN = -Wall

CFLAGS=-ggdb

O = faSplit.o 

faSplit: ${O}
	${CC} ${COPT} -ggdb -o ${BINDIR}/faSplit $O \
	../../lib/${MACHTYPE}/jkweb.a
	${STRIP} ${BINDIR}/faSplit${EXE}

debug:
	${CC} ${COPT} -o faSplit $O ../../lib/${MACHTYPE}/jkweb.a

clean::
	rm -f ${O} faSplit


