include ../../../inc/common.mk

# For performance
#.c.o:
#	${CC} ${COPT} ${CFLAGS} -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../../inc -I../../../inc -I../../../../inc -c $*.c

#For debugging:
.c.o:
	${CC} ${COPT} ${CFLAGS} -Wformat -Wimplicit  -Wreturn-type -I../../inc -I../../../inc -I../../../../inc -c $*.c

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = hgCGAP

O = $A.o

all: compile
	${STRIP} $A${EXE}
	mv $A $$HOME/bin/$(MACHTYPE)/$A

compile: $O 
	${CC} ${COPT} ${CFLAGS} $O $(MYLIBS) $(L) 
	mv a.out $A
	chmod a+rx $A

lib:
	cd ../../../lib; make

tags: 
	ctags *.h *.c ../../lib/*.c ../../inc/*.h ../../../lib/*.c ../../../inc/*.h

