include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = pslPretty.o

pslPretty: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/pslPretty $O $(MYLIBS) $L
#strip $(HOME)/bin/$(MACHTYPE)/pslPretty
test:: 
	${MKDIR} output
	pslPretty -long -axt input/test1.psl input/S1.lst input/S2.lst output/test1.axt
