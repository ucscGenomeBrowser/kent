.c.o:
	gcc -ggdb -O -Wimplicit -I../inc -I../../inc -I../../../inc -c $*.c

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgSeqSearch.o

hgSeqSearch: $O $(MYLIBS)
	gcc -ggdb -o hgSeqSearch $O $(MYLIBS) $L
	strip hgSeqSearch
	chmod a+rx hgSeqSearch
	mv hgSeqSearch /usr/local/apache/cgi-bin
