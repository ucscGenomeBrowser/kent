.c.o:
	gcc -O -I/usr/include/mysql -I../../inc -c $*.c

L = /usr/lib/mysql/libmysqlclient.a -lm
O = jksql.o 

jksql: $(O)
	gcc -o jksql $O ../../lib/$(MACHTYPE)/jkweb.a $L

