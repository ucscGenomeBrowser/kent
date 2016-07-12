kentSrc = ../../..
include ../../../inc/common.mk

L += 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = howBadNt.o

howBadNt: $O 
	${CC} ${COPT} ${CFLAGS} -o howBadNt $O $(MYLIBS) $L

clean::
	rm -f ${O}
