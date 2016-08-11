kentSrc = ../../../..
include ../../../../inc/common.mk

L += $(MYSQLLIBS)
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = mm.o

mm: $O 
	${CC} ${COPT} ${CFLAGS} -o mm $O $(MYLIBS) $L

clean::
	rm -f ${O}
