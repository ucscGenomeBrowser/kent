kentSrc = ../..
include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = sampleToWig.o

sampleToWig: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/sampleToWig $O $(MYLIBS) $L
#	${STRIP} ${BINDIR}/sampleToWig${EXE}

clean::
	rm -f ${O}
