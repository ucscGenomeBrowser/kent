include ../../inc/common.mk


L = $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ooCloneSizes.o

ooCloneSizes: $O 
	gcc -o $(HOME)/bin/$(MACHTYPE)/ooCloneSizes $O $(MYLIBS) $L

