include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = stageMultiz.o

stageMultiz: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/stageMultiz $O $(MYLIBS) $L
#	${STRIP} $(HOME)/bin/$(MACHTYPE)/stageMultiz${EXE}

tags: 
	ctags *.h *.c ../../lib/*.c ../../inc/*.h ../../../lib/*.c ../../../inc/*.h
