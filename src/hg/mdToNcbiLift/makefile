include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = mdToNcbiLift.o

mdToNcbiLift: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/mdToNcbiLift $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/mdToNcbiLift${EXE}
