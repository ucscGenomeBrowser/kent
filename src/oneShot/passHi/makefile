include ../../inc/common.mk


O =  passHi.o

${BINDIR}/passHi: $(O)  
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/passHi $O ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
