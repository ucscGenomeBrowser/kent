.c.o:
	gcc -ggdb -Wimplicit -I../../inc -c $*.c

L = /usr/lib/mysql/libmysqlclient.a -lm

O = superStitcher.o  

superStitcher: $O ../../lib/$(MACHTYPE)/jkweb.a
	gcc -ggdb -o superStitcher $O ../../lib/$(MACHTYPE)/jkweb.a $L

lib:
	cd ../../lib; make
