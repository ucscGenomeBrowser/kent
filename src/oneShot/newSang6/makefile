include ../../inc/common.mk


L = $(MYSQLLIBS) -lm

O = newSang6.o

newSang6: $O ../../lib/$(MACHTYPE)/jkweb.a
	gcc -ggdb -o newSang6 $O ../../lib/$(MACHTYPE)/jkweb.a $L

lib:
	cd ../../lib; make
