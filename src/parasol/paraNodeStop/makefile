include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = paraNodeStop.o

paraNodeStop: $O $(MYLIBS)
	gcc -o ../bin/paraNodeStop $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a
	strip ../bin/paraNodeStop
	chmod 744 ../bin/paraNodeStop
