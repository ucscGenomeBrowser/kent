include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

test: output spDbTest hdbTest
	${MKDIR} output
	rm -r output
	echo tested all

spDbTest: spDbTest.o $(MYLIBS)
	gcc -ggdb -o bin/$(MACHTYPE)/spDbTest spDbTest.o $(MYLIBS) $L
	strip bin/$(MACHTYPE)/spDbTest
	bin/$(MACHTYPE)/spDbTest swissProt Q9FFH7 > output/spDbTest
	bin/$(MACHTYPE)/spDbTest swissProt P29312 >> output/spDbTest
	diff expected/spDbTest output/spDbTest

hdbTest: hdbTest.o $(MYLIBS)
	gcc -ggdb -o bin/$(MACHTYPE)/hdbTest hdbTest.o $(MYLIBS) $L
	strip bin/$(MACHTYPE)/hdbTest
	bin/$(MACHTYPE)/hdbTest > output/hdbTest
	diff expected/hdbTest output/hdbTest
	
