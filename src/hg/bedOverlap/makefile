include ../../inc/common.mk


L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = bedOverlap.o

bedOverlap: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/bedOverlap $O $(MYLIBS) $L
#	strip $(HOME)/bin/$(MACHTYPE)/bedOverlap
