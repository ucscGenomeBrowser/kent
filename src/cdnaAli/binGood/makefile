include ../../inc/common.mk


O =  binGood.o ../lib/cdnaAli.o

${BINDIR}/binGood: $(O)  
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/binGood $O ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
