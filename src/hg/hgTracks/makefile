.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -c $*.c

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgTracks.o hgFind.o

hgTracks: $O 
	gcc $O $(MYLIBS) $(L) 
	mv a.out hgTracks
	strip hgTracks
	chmod a+rx hgTracks
	mv hgTracks /usr/local/apache/cgi-bin

lib:
	cd ../../lib; make

tags: 
		ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h
