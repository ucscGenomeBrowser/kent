include ../../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = chainToAxt.o

chainToAxt: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/chainToAxt $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/chainToAxt

test: chrM
%:	input/%.chain
	mkdir -p output
	chainToAxt input/$@.chain /gbdb/hg16/nib /gbdb/mm3/nib output/$@.axt
	diff output/$@.axt expected/$@.axt
	rm output/$@.axt
