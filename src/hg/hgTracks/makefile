.c.o:
	gcc -ggdb -I../inc -Wimplicit -I../../inc -c $*.c

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgTracks.o hgFind.o

hgTracks: $O 
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a $(MYLIBS) $L
	mv a.out hgTracks
	strip hgTracks
	chmod a+rx hgTracks
	chmod g-w hgTracks
	mv hgTracks /usr/local/apache/cgi-bin

lib:
	cd ../../lib; make
