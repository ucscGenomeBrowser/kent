include ../../inc/common.mk


L = -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = paraNodeStart.o

paraNodeStart: $O $(MYLIBS)
	gcc -o ../bin/paraNodeStart $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a
	strip ../bin/paraNodeStart
