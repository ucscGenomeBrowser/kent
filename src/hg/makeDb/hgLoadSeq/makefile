include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgLoadSeq.o 

hgLoadSeq: $O ../../../lib/$(MACHTYPE)/jkweb.a makefile
	${CC} ${COPT} ${CFLAGS} ${F64} -o ${BINDIR}/hgLoadSeq $O $(MYLIBS) $L

lib:
	cd ../../../lib && ${MAKE}

clean::
	rm -f ${O} hgLoadSeq
