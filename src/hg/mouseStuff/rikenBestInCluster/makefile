include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = rikenBestInCluster.o

rikenBestInCluster: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/rikenBestInCluster $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/rikenBestInCluster
