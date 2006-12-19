include ../../inc/common.mk

O = faRandomize.o 

faNoise: $(O)
	${CC} ${COPT} ${CFLAGS} -o faRandomize $O ../../lib/$(MACHTYPE)/jkweb.a
	cp faRandomize ${BINDIR}/

