kentSrc = ../..
include ../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS} 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
#COPT = -ggdb

A = pslAnal

O = $A.o

compile: $O 
	${CC} ${COPT} ${CFLAGS} $O $(MYLIBS) $(L) 
	mv ${AOUT} $A${EXE}
	chmod a+rx $A${EXE}
	cp -f $A ${BINDIR}/$A${EXE}

debug: $O
	${CC} ${COPT} ${CFLAGS} $O $(MYLIBS) $(L)
	mv ${AOUT} $A${EXE}

lib:
	cd ../../lib && ${MAKE}

tags:  
	ctags *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h

clean::
	rm -f ${O}
