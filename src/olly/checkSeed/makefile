include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = checkSeed.o

checkSeed: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/checkSeed $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/checkSeed${EXE}
