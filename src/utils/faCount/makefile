include ../../inc/common.mk

O = faCount.o 

faCount: ${O}
	${CC} -o ${BINDIR}/faCount $O ../../lib/${MACHTYPE}/jkweb.a -lm

clean::
	rm -f ${O}
