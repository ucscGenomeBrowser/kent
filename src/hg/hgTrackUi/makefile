.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -I../../../inc -c $*.c

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgTrackUi.o

hgTrackUi: $O $(MYLIBS)
	gcc -ggdb -o hgTrackUi $O $(MYLIBS) $L
	strip hgTrackUi
	chmod a+rx hgTrackUi
	mv hgTrackUi $(HGCGI)
