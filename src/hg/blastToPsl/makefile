include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = blastToPsl
O = $A.o 

my: compile scoreSort blast0To8 smashTab blastLink getFaList tryToPsl blast0ToPsl
	mv $A $(HOME)/bin/$(MACHTYPE)
	mv scoreSort $(HOME)/bin/$(MACHTYPE)
	mv blast0To8 $(HOME)/bin/$(MACHTYPE)
	mv smashTab $(HOME)/bin/$(MACHTYPE)
	mv getFaList $(HOME)/bin/$(MACHTYPE)
	mv tryToPsl $(HOME)/bin/$(MACHTYPE)
	mv blast0ToPsl $(HOME)/bin/$(MACHTYPE)

blast0ToPsl: blast0ToPsl.o
	${CC} blast0ToPsl.o ${F64} ${MYLIBS} ${L} 
	mv a.out blast0ToPsl
tryToPsl: tryToPsl.o
	${CC} tryToPsl.o ${F64} ${MYLIBS} ${L} 
	mv a.out tryToPsl
getFaList: getFaList.o
	${CC} getFaList.o ${F64} ${MYLIBS} ${L} 
	mv a.out getFaList
blastLink: blastLink.o
	${CC} blastLink.o ${F64} ${MYLIBS} ${L} 
	mv a.out blastLink
smashTab: smashTab.o
	${CC} smashTab.o ${F64} ${MYLIBS} ${L} 
	mv a.out smashTab
blast0To8: blast0To8.o
	${CC} blast0To8.o ${F64} ${MYLIBS} ${L} 
	mv a.out blast0To8

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
