.c.o:
	gcc -ggdb -Wimplicit -I../inc -I../../inc -c $*.c

L = /usr/lib/mysql/libmysqlclient.a -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = loadSeq.o

loadSeq: $O ../../lib/$(MACHTYPE)/jkweb.a
	gcc -o $(HOME)/bin/$(MACHTYPE)/loadSeq $O $(MYLIBS) $L

lib:
	cd ../../lib; make
