include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = sampleToWig.o

sampleToWig: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/sampleToWig $O $(MYLIBS) $L
#	${STRIP} $(HOME)/bin/$(MACHTYPE)/sampleToWig${EXE}
