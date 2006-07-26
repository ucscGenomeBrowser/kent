include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgPhMouse.o

hgPhMouse: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/hgPhMouse $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/hgPhMouse${EXE}
