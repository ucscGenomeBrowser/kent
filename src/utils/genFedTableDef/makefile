
include ../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = genFedTableDef

O = $A.o

my: compile
	mv $A ${BINDIR}/$A

mgsc: compile
	mv $A /usr/local/apache/cgi-bin-mgsc/$A

alpha: compile
	mv $A /usr/local/apache/cgi-bin/$A

beta: compile
	mv $A /usr/local/apache/cgi-bin-beta/$A


compile: $O 
	${CC} ${COPT} ${CFLAGS} $O $(MYLIBS) $(L) 
	mv ${AOUT} $A${EXE}
#	${STRIP} $A${EXE}
	chmod a+rx $A

lib:
	cd ../../lib && ${MAKE}

tags: 
	ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h

