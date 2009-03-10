include ../../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgRnaGenes.o

hgRnaGenes: $O ../../../../lib/$(MACHTYPE)/jkweb.a
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/hgRnaGenes $O $(MYLIBS) $L

lib:
	cd ../../../../lib && ${MAKE}

clean::
	rm -f ${O} hgRnaGenes
