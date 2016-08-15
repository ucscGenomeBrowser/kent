kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = clonesInAgp.o

clonesInAgp: $O 
	${CC} ${COPT} ${CFLAGS} -o clonesInAgp $O $(MYLIBS) $L

clean::
	rm -f ${O}
