include ../../inc/common.mk


L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = liftUp.o

all: liftUp

liftUp: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/liftUp $O $(MYLIBS) $L

test: force
	cd test && ${MAKE}

force: 

