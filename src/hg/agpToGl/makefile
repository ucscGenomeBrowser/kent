include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = agpToGl.o

agpToGl: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/agpToGl $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/agpToGl${EXE}
