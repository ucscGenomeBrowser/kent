include ../../inc/common.mk


L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = pslSimp.o 

pslSimp: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/pslSimp $O $(MYLIBS) $L

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f ${O} pslSimp
