include ../../inc/common.mk


L = /usr/lib/mysql/libmysqlclient.a -lm

O = tabTable.o 

tabTable: $(O)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/tabTable $O ../../lib/$(MACHTYPE)/jkweb.a $L

