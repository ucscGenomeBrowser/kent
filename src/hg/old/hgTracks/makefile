.c.o:
	gcc -ggdb -I../inc -I../../inc -c $*.c

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgTracks.o

hgTracks: $O ../../lib/$(MACHTYPE)/jkweb.a
	gcc $O $(MYLIBS) $L
	mv a.out hgTracks
	strip hgTracks
	chmod a+rx hgTracks
	chmod g-w hgTracks
	mv hgTracks /usr/local/apache/cgi-bin

lib:
	cd ../../lib; make
