include ../../inc/common.mk

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
#COPT = -ggdb

A = pslAnal

O = $A.o

compile: $O 
	${CC} ${COPT} ${CFLAGS} $O $(MYLIBS) $(L) 
	mv a.out $A
	chmod a+rx $A
	cp -f $A $$HOME/bin/i386/$A
	#${STRIP} $$HOME/bin/i386/$A${EXE}

debug: $O
	${CC} ${COPT} ${CFLAGS} $O $(MYLIBS) $(L)
	mv a.out $A

lib:
	cd ../../lib; make

tags:  
	ctags *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h
