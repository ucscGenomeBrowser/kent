include ../../inc/common.mk


O =  cluster.o ../lib/cdnaAli.o

L = ../../lib/$(MACHTYPE)/jkweb.a

${BINDIR}/cluster: $(O)  $(L)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/cluster $O $(L)


