
L += $(MYSQLLIBS) -lm 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a ../lib/$(MACHTYPE)/libSpliceGraph.a

% : %.c
	${CC} ${COPT} ${CFLAGS} -Wimplicit -I../inc -I../../inc  -I../../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) ${L} $(MYSQLLIBS) -lm
	cp $@ ${BINDIR}

%Test: %Test.c 
	${CC} ${COPT} ${CFLAGS} -Wimplicit  -I../inc -I../../inc -I../../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) ${L}

O = spliceWalk

TESTO = 

TESTS = 

exe: $(O)


test: $(TESTS)

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f ${O} ${O}.o
