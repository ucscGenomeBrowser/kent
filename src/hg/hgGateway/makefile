.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -I../../../inc -c $*.c

L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgGateway.o

hgGateway: $O $(MYLIBS)
	gcc -ggdb -o hgGateway $O $(MYLIBS) $L
	strip hgGateway
	chmod a+rx hgGateway
	mv hgGateway $(HGCGI)
