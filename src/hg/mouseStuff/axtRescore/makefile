include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = axtRescore.o

axtRescore: compile
	${STRIP} $(HOME)/bin/$(MACHTYPE)/axtRescore${EXE}

compile: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/axtRescore $O $(MYLIBS) $L
