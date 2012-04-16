include ../../inc/common.mk


O =  cluster.o ../lib/cdnaAli.o

JKWEB=../../lib/$(MACHTYPE)/jkweb.a

${BINDIR}/cluster: $(O) $(JKWEB)
	${CC} ${COPT} ${CFLAGS} -lm -o ${BINDIR}/cluster $O ${JKWEB} ${L}

clean::
	rm -f ${O}
