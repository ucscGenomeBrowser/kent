include ../../inc/common.mk


L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = bedOverlap.o

bedOverlap: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/bedOverlap $O $(MYLIBS) $L
#	${STRIP} $(HOME)/bin/$(MACHTYPE)/bedOverlap${EXE}

test:	test2 test4
%:	input/%.bed
	${MKDIR} output
	bedOverlap -hasBin -verbose=3 input/$@.bed output/$@.bed  
	diff expected/$@.bed output/$@.bed 
	rm output/$@.bed
%:	input/%.noBin
	${MKDIR} output
	bedOverlap input/$@.noBin output/$@.bed 
	diff expected/$@.bed output/$@.bed 
	rm output/$@.bed
