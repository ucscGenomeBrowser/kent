include ../../inc/common.mk


O =  cluster.o ../lib/cdnaAli.o

JKWEB=../../lib/$(MACHTYPE)/jkweb.a
L += ${JKWEB}

${BINDIR}/cluster: $(O) $(JKWEB)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/cluster $O $(L)

clean::
	rm -f ${O}
