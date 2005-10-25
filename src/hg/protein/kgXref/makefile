include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = kgXref

O = $A.o

my: compile
	mv $A ${BINDIR}

compile: $O 
	${CC} ${COPT} ${CFLAGS} $O $(MYLIBS) $(L) 
	mv ${AOUT} $A${EXE}
	${STRIP} $A${EXE}
	chmod a+rx $A

lib:
	cd ../../lib && ${MAKE}

tags: 
	ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h

