include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = axtBestNear.o

axtBestNear: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/axtBestNear $O $(MYLIBS) $L
#	strip $(HOME)/bin/$(MACHTYPE)/axtBestNear
test: chr22 
%:	input/%.axt
	axtBestNear input/$@.axt $@ output/$@.axt 
	diff expected/$@.axt output/$@.axt 
	rm output/$@.axt
