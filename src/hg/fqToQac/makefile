include ../../inc/common.mk


L = -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = fqToQac.o

fqToQac: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/fqToQac $O $(MYLIBS) $L


lib:
	cd ../lib && ${MAKE}
	cd ../../lib && ${MAKE}
