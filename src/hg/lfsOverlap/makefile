include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = lfsOverlap.o

pslSelect: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/lfsOverlap $O $(MYLIBS) $L
test:
	cd tests && ${MAKE} test

clean:
	rm -f $O 
	cd tests && ${MAKE} clean
