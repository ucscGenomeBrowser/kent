.c.o:
	gcc -ggdb -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -c $*.c

MYSQLLIBS = /usr/local/mysql/lib/mysql/libmysqlclient.a
L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgc.o

hgc: $O ../../lib/$(MACHTYPE)/jkweb.a 
	gcc $O $(MYLIBS) $L
	mv a.out hgc
	strip hgc
	chmod a+rx hgc
	mv hgc /usr/local/apache/cgi-bin

tags: hgc.c 
		ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h
