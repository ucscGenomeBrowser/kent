kentSrc = ../../..
include ../../../inc/common.mk


L += -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

O = barger.o 

barger: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/barger $O $(MYLIBS) $L


lib:
	cd ../../lib && ${MAKE}
	cd ../../../lib && ${MAKE}

clean::
	rm -f ${O}
