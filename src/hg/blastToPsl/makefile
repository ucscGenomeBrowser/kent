include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = blastToPsl
O = $A.o 

my: compile scoreSort
	mv $A $(HOME)/bin/$(MACHTYPE)
	mv scoreSort $(HOME)/bin/$(MACHTYPE)

scoreSort: scoreSort.o
	${CC} scoreSort.o ${F64} ${MYLIBS} ${L} 
	mv a.out scoreSort

compile: $O 
	${CC} $O ${F64} ${MYLIBS} ${L} 
	mv a.out $A
#	strip $A
	chmod a+rx $A


debug: $O
	${CC} $O ${MYLIBS} ${L} 
	mv a.out $A

test:	compile
	cd test && ${MAKE}

clean:
	cd test && ${MAKE} clean
	rm -f $O $A scoreSort.o

tags: 
	ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h
