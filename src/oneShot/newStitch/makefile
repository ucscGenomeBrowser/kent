include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = newStitch.o

newStitch: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/newStitch $O $(MYLIBS) $L
#	${STRIP} $(HOME)/bin/$(MACHTYPE)/newStitch${EXE}
