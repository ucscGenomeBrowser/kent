kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = tempLift.o

tempLift: $O 
	${CC} ${COPT} ${CFLAGS} -o tempLift $O $(MYLIBS) $L

clean::
	rm -f ${O}
