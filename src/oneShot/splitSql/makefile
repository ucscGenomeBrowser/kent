include ../../inc/common.mk


O = splitSql.o 

splitSql: $(O)
	${CC} ${COPT} ${CFLAGS} ${F64} -o ${BINDIR}/splitSql $O ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
