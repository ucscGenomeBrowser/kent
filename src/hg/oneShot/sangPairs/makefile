kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = sangPairs.o

sangPairs: $O 
	${CC} ${COPT} ${CFLAGS} -o sangPairs $O $(MYLIBS) $L

clean::
	rm -f ${O}
