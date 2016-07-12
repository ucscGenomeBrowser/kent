kentSrc = ../../../..
include ../../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ppSanger.o sangInsert.o sangRange.o sangRead.o

ppSanger: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ppSanger $O $(MYLIBS) $L
	mv ppSanger ${BINDIR}


lib:
	cd ../../lib && ${MAKE}
	cd ../../../lib && ${MAKE}

clean::
	rm -f ${O}
