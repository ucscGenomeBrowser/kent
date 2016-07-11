kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = clonePos.o

clonePos: $O 
	${CC} ${COPT} ${CFLAGS} -o clonePos $O $(MYLIBS) $L

clean::
	rm -f ${O}
