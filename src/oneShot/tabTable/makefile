.c.o:
	gcc -O -I../../inc -c $*.c

L = /usr/lib/mysql/libmysqlclient.a -lm

O = tabTable.o 

tabTable: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/tabTable $O ../../lib/$(MACHTYPE)/jkweb.a $L

