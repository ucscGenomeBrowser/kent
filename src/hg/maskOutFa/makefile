include ../../inc/common.mk


L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = maskOutFa.o

maskOutFa: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/maskOutFa $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/maskOutFa
