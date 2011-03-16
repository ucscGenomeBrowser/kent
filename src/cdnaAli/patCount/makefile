include ../../inc/common.mk

O =  patCount.o

${BINDIR}/patCount: $(O)  
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/patCount $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
