include ../../inc/common.mk


L = $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ooCloneOverlaps.o

ooCloneOverlaps: $O 
	gcc -o $(HOME)/bin/$(MACHTYPE)/ooCloneOverlaps $O $(MYLIBS) $L

