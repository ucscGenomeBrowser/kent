include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = pslPartition.o

${BINDIR}/pslPartition: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o pslPartition $O $(MYLIBS) $L
	cp -f pslPartition $@

test:
	(cd tests && ${MAKE} test)

clean::
	rm -f pslPartition ${O}
