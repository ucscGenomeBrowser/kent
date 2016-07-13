kentSrc = ../../../..
include ../../../../inc/common.mk

L += 
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = checkDupe.o

checkDupe: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o checkDupe $O $(MYLIBS) $L

lib:
	cd ../../../lib && ${MAKE}
	cd ../../../../lib && ${MAKE}

clean::
	rm -f ${O} checkDupe
