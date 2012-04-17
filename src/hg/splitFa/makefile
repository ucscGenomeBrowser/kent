include ../../inc/common.mk

L += 
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = splitFa.o 

splitFa: $(O)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/splitFa $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
