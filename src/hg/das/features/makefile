.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -I../../../inc -c $*.c

L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = features.o

features: $O $(MYLIBS)
	gcc -ggdb -o features $O $(MYLIBS) $L
	strip features
	mv features /usr/local/apache/cgi-bin/das/DSN/features
