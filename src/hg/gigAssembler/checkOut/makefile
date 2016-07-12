kentSrc = ../../..
include ../../../inc/common.mk


L += 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = checkOut.o 

checkOut: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o checkOut $O $(MYLIBS) $L


lib:
	cd ../../lib && ${MAKE}
	cd ../../../lib && ${MAKE}

clean::
	rm -f ${O}
