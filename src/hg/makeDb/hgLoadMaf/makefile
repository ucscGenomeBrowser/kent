include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgLoadMaf.o

hgLoadMaf: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/hgLoadMaf $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/hgLoadMaf${EXE}

test:
	cd tests && ${MAKE} test

clean:
	rm -f $O $B
	cd tests && ${MAKE} clean

