
CFLAGS= -ggdb -O -Wimplicit

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = align.o hgCountAlign.o slidingWin.o  winCounts.o
H = align.h  slidingWin.h  winCounts.h
PROG = $(HOME)/bin/$(MACHTYPE)/hgCountAlign

$(PROG): $O ../../../lib/$(MACHTYPE)/jkweb.a
	gcc $(CFLAGS) -o $(PROG) $O $(MYLIBS) $L

$O: $H

.c.o:
	gcc $(CFLAGS) -I../../inc -I../../../inc -c $*.c

clean::
	rm -rf $O output

test:: test1 test2 test3 test4

test1:
	@mkdir -p output
	$(PROG) test.axt output/test1.tab
	diff expected/test1.tab output/test1.tab

test2:
	@mkdir -p output
	$(PROG) -winSize=200 -winSlide=50 test.axt output/test2.tab
	diff expected/test2.tab output/test2.tab

test3:
	@mkdir -p output
	$(PROG) -winSize=200 -winSlide=50 -fixedNumCounts=50 -countCoords \
	     test.axt output/test3.tab
	diff expected/test3.tab output/test3.tab

test4:
	@mkdir -p output
	$(PROG) -winSize=200 -winSlide=50 -selectBed=test-sel.bed \
	    test.axt output/test4.tab
	diff expected/test4.tab output/test4.tab

