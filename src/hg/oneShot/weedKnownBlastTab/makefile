include ../../../inc/common.mk

L = -lm $(MYSQLLIBS) 
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = weedKnownBlastTab.o

weedKnownBlastTab: $O $(MYLIBS)
	gcc -ggdb -o weedKnownBlastTab $O $(MYLIBS) $L
	strip weedKnownBlastTab
