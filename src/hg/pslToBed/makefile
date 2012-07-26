include ../../inc/common.mk


L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = pslToBed.o 

myown:pslToBed
	mv pslToBed ${BINDIR}/

pslToBed: $O
	${CC} ${COPT} ${CFLAGS} -Wimplicit  -I../inc -I../../inc  -o $@ $(addsuffix .c,$@) $(MYLIBS) $(L)


test: pslToBed
	cd tests && ${MAKE} test

lib:
	cd ../../lib && ${MAKE}

clean::
	rm -f ${O}
