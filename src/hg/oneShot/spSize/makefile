kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = spSize.o

spSize: $O 
	${CC} ${COPT} ${CFLAGS} -o spSize $O $(MYLIBS) $L

clean::
	rm -f ${O}
