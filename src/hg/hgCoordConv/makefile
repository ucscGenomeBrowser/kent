.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -c $*.c

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgCoordConv.o

hgCoordConv: $O 
	gcc $O $(MYLIBS) $(L) 
	mv a.out hgCoordConv
	strip hgCoordConv
	chmod a+rx hgCoordConv
	mv hgCoordConv $(HGCGI)

lib:
	cd ../../lib; make

tags: 
		ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h
