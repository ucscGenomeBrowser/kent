include ../../inc/common.mk


L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = pslStats.o

pslStats: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/pslStats $O $(MYLIBS) $L
test:
	cd tests && ${MAKE} test

clean:
	rm -f $O 
	cd tests && ${MAKE} clean
