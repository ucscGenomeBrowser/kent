include ../../inc/common.mk


O = blastStats.o

blastStats: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/blastStats $O ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
