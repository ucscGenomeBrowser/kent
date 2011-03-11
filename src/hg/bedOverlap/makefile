include ../../inc/common.mk


L += -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = bedOverlap.o

bedOverlap: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/bedOverlap $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/bedOverlap${EXE}

test:	test2 test4 test5
%:	input/%.bed
	${MKDIR} output
	bedOverlap -hasBin -verbose=1 input/$@.bed output/$@.bed  
	diff expected/$@.bed output/$@.bed 
	rm output/$@.bed
%:	input/%.noBin
	${MKDIR} output
	bedOverlap input/$@.noBin output/$@.bed 
	diff expected/$@.bed output/$@.bed 
	rm output/$@.bed

clean::
	rm -fr ${O} output
