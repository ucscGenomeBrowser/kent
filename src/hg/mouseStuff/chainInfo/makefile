include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = chainInfo.o

chainInfo: $O $(MYLIBS)
	gcc -ggdb -O -o $(HOME)/bin/$(MACHTYPE)/chainInfo $O $(MYLIBS) $L
#	strip $(HOME)/bin/$(MACHTYPE)/chainInfo
