kentSrc = ../..

include ../../inc/common.mk

L += $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

E = mgcFastaForBed agxToIntronBeds bedToFrames orthoMap orthoEvaluate orthoPickIntron # spliceStats spliceVis

O = orthoEval.o

${DESTDIR}${BINDIR}/%: %.c $(O)
	$(CC) $(COPTS) -I../inc  -I../../inc  -o $@ $< $(O) $(MYLIBS) $L -lm -lz

%Test: %Test.c 
	${CC} ${COPT} ${CFLAGS} -Wimplicit  -I../inc -I../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) $(L)

compile: $(E:%=${DESTDIR}${BINDIR}/%)

test: $(TESTS)

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f ${E} *.o
