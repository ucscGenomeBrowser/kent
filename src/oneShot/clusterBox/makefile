include ../../inc/common.mk


L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = clusterBox.o boxClump.o

clusterBox: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/clusterBox $O $(MYLIBS) $L
#	strip $(HOME)/bin/$(MACHTYPE)/clusterBox
