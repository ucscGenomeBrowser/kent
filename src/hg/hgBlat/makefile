.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -c $*.c

L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgBlat.o

hgBlat: $O $(MYLIBS)
	gcc -ggdb -o hgBlat $O $(MYLIBS) $L
	strip hgBlat
	chmod a+rx hgBlat
	mv hgBlat $(HGCGI)
