include ../../inc/common.mk


L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = checkTableCoords
O = $A.o 

all: compile install
compile: $A

$A: $O $(MYLIBS)
	gcc -ggdb -o $A $O $(MYLIBS) $L

install:
	strip $A
	mv $A $(HOME)/bin/$(MACHTYPE)/$A
