kentSrc = ../..
include ../../inc/common.mk


L += /usr/lib/mysql/libmysqlclient.a -lm
O = jksql.o 

jksql: $(O)
	${CC} ${COPT} ${CFLAGS} -o jksql $O ../../lib/$(MACHTYPE)/jkweb.a $L

clean::
	rm -f ${O}
