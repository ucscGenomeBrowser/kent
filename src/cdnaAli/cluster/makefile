include ../../inc/common.mk


O =  cluster.o ../lib/cdnaAli.o

L = ../../lib/$(MACHTYPE)/jkweb.a

$(HOME)/bin/$(MACHTYPE)/cluster: $(O)  $(L)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/cluster $O $(L)


