include ../../inc/makefile.h


L = /usr/lib/mysql/libmysqlclient.a -lm
O = jksql.o 

jksql: $(O)
	gcc -o jksql $O ../../lib/$(MACHTYPE)/jkweb.a $L

