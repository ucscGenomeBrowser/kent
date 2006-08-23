include ../../../inc/common.mk


L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = checkAgpAndFa.o
A = ${BINDIR}/checkAgpAndFa

checkAgpAndFa: $O 
	${CC} ${COPT} ${CFLAGS} -o $A $O $(MYLIBS) $L
	${STRIP} $A${EXE}

test:
	./doTest.sh $A

clean:
	rm -rf *.o ./checkAgpAndFa tests/output
