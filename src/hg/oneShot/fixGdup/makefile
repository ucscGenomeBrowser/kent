kentSrc = ../../..
include ../../../inc/common.mk

L += $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = fixGdup.o

fixGdup: $O 
	${CC} ${COPT} ${CFLAGS} -o fixGdup $O $(MYLIBS) $L

clean::
	rm -f ${O}
