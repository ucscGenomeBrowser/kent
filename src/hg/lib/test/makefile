include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

test: spDbTest
	echo tested all

spDbTest: spDbTest.o $(MYLIBS)
	gcc -ggdb -o bin/$(MACHTYPE)/spDbTest spDbTest.o $(MYLIBS) $L
	strip bin/$(MACHTYPE)/spDbTest
	mkdir -p output
	bin/$(MACHTYPE)/spDbTest swissProt Q9FFH7 > output/spDbTest
	bin/$(MACHTYPE)/spDbTest swissProt P29312 >> output/spDbTest
	diff expected/spDbTest output/spDbTest
	rm output/spDbTest
