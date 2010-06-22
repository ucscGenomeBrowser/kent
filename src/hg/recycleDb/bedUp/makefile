include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = bedUp.o

bedUp: $O ../../../lib/$(MACHTYPE)/jkweb.a
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/bedUp $O $(MYLIBS) $L

lib:
	cd ../../../lib && ${MAKE}

clean::
	rm -f ${O}
