
# For performance
#.c.o:
#	${CC} ${COPT} ${CFLAGS} -O -Wformat -Wimplicit -Wuninitialized -Wreturn-type -I../inc -I../../inc -I../../../inc -c $*.c

#For debugging:
.c.o:
	${CC} ${COPT} ${CFLAGS} -Wformat -Wimplicit  -Wreturn-type -I../inc -I../../inc -I../../../inc -c $*.c

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = kgProtAliasNCBI

O = $A.o

my: compile
	mv $A /cluster/home/$(USER)/bin/$(MACHTYPE)

compile: $O 
	${CC} ${COPT} ${CFLAGS} $O $(MYLIBS) $(L) 
	mv a.out $A
	${STRIP} $A${EXE}
	chmod a+rx $A

lib:
	cd ../../lib; make

tags: 
	ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h

