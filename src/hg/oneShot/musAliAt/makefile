include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm 
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = musAliAt.o

musAliAt: $O $(MYLIBS)
	gcc -ggdb -o musAliAt $O $(MYLIBS) $L
