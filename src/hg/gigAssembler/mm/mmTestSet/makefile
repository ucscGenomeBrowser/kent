kentSrc = ../../../..
include ../../../../inc/common.mk

L += $(MYSQLLIBS)
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = mmTestSet.o

mmTestSet: $O 
	${CC} ${COPT} ${CFLAGS} -o mmTestSet $O $(MYLIBS) $L

clean::
	rm -f ${O}
