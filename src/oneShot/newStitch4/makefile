include ../../inc/common.mk


L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = newStitch4.o 

newStitch4: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/newStitch4 $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/newStitch4${EXE}
