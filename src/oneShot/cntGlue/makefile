include ../../inc/common.mk


O =  cntGlue.o

${BINDIR}/cntGlue: $(O)  
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/cntGlue $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
