include ../../inc/makefile.h


L = /usr/lib/mysql/libmysqlclient.a -lm

O = tabTable.o 

tabTable: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/tabTable $O ../../lib/$(MACHTYPE)/jkweb.a $L

