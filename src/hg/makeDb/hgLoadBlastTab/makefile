include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgLoadBlastTab.o

hgLoadBlastTab: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgLoadBlastTab $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/hgLoadBlastTab
