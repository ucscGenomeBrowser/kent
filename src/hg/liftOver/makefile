include ../../inc/common.mk


L = $(MYSQLLIBS) -lm 
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = liftOver.o

liftOver: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/liftOver $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/liftOver
