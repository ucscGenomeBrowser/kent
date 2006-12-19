
include ../../inc/common.mk

L = $(MYSQLLIBS) -lm 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

% : %.c
	$(CC) $(COPTS) -I../inc  -I../../inc  -o $@ $(addsuffix .c,$@) $(O) $(MYLIBS) $(MYSQLLIBS) -lm -lz
	cp $@ ${BINDIR}

%Test: %Test.c 
	${CC} ${COPT} ${CFLAGS} -Wimplicit  -I../inc -I../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) $(L)

E = mgcFastaForBed agxToIntronBeds bedToFrames orthoMap orthoEvaluate orthoPickIntron # spliceStats spliceVis

O = orthoEval.o

exe: compile $(E) 

compile: $(O)

test: $(TESTS)

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f ${E} *.o
