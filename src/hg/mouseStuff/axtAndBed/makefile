include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = axtAndBed.o

axtAndBed: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/axtAndBed $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/axtAndBed${EXE}
