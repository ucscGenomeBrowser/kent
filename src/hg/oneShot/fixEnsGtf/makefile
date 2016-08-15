kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = fixEnsGtf.o

fixEnsGtf: $O 
	${CC} ${COPT} ${CFLAGS} -o fixEnsGtf $O $(MYLIBS) $L

clean::
	rm -f ${O}
