include ../../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ggcPic.o

ggcPic: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/ggcPic $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/ggcPic${EXE}
