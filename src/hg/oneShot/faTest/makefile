kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = faTest.o

faTest: $O 
	${CC} ${COPT} ${CFLAGS} -o faTest $O $(MYLIBS) $L

clean::
	rm -f ${O}
