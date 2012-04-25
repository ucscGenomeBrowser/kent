include ../../inc/common.mk

O =  ali2alx.o ../lib/cdnaAli.o

${BINDIR}/ali2alx: $(O)  
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/ali2alx $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
