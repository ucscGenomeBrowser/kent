include ../../inc/common.mk


L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = faTrimRead.o

faTrimRead: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/faTrimRead $O $(MYLIBS) $L

clean:
	rm -f $O

test: test1
%:	input/%.fa
	mkdir -p output
	faTrimRead input/$@.fa input/$@.qual output/$@.fa
	diff expected/$@.fa output/$@.fa
	rm output/$@.fa
