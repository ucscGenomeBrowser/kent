.c.o:
	gcc -ggdb -I../inc -Wimplicit -I../../inc -c $*.c

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hTracks.o 

hTracks: $O ../../lib/$(MACHTYPE)/jkweb.a
	gcc $O $(MYLIBS) $L
	mv a.out hTracks
	strip hTracks
	chmod a+rx hTracks
	chmod g-w hTracks
	mv hTracks /usr/local/apache/cgi-bin

lib:
	cd ../../lib; make
