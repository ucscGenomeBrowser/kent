include ../../inc/common.mk


L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = liftUp.o

liftUp: $O ../../lib/$(MACHTYPE)/jkweb.a 
	gcc -o $(HOME)/bin/$(MACHTYPE)/liftUp $O $(MYLIBS) $L

