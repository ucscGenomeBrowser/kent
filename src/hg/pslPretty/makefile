include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = pslPretty.o

pslPretty: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/pslPretty $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/pslPretty
test: test1
%:	input/%.psl
	${MKDIR} output
	pslPretty -axt input/$@.psl input/S1.lst input/S2.lst output/$@.axt
